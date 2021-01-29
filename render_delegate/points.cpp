// Copyright 2019 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "points.h"

#include "constant_strings.h"
#include "material.h"
#include "utils.h"

PXR_NAMESPACE_OPEN_SCOPE

HdArnoldPoints::HdArnoldPoints(HdArnoldRenderDelegate* delegate, const SdfPath& id, const SdfPath& instancerId)
    : HdArnoldGprim<HdPoints>(str::points, delegate, id, instancerId)
{
}

HdDirtyBits HdArnoldPoints::GetInitialDirtyBitsMask() const
{
    return HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyTransform | HdChangeTracker::DirtyVisibility |
           HdChangeTracker::DirtyPrimvar | HdChangeTracker::DirtyWidths | HdChangeTracker::DirtyMaterialId |
           HdArnoldShape::GetInitialDirtyBitsMask();
}

void HdArnoldPoints::Sync(
    HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits, const TfToken& reprToken)
{
    auto* param = reinterpret_cast<HdArnoldRenderParam*>(renderParam);
    const auto& id = GetId();
    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)) {
        param->Interrupt();
        HdArnoldSetPositionFromPrimvar(GetShape(), id, delegate, str::points);
        // HdPrman exports points like this, but this method does not support
        // motion blurred points.
    } else if (*dirtyBits & HdChangeTracker::DirtyPoints) {
        param->Interrupt();
        const auto pointsValue = delegate->Get(id, HdTokens->points);
        if (!pointsValue.IsEmpty() && pointsValue.IsHolding<VtVec3fArray>()) {
            const auto& points = pointsValue.UncheckedGet<VtVec3fArray>();
            auto* arr = AiArrayAllocate(points.size(), 1, AI_TYPE_VECTOR);
            AiArraySetKey(arr, 0, points.data());
            AiNodeSetArray(GetShape(), str::points, arr);
        }
    }

    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->widths)) {
        param->Interrupt();
        HdArnoldSetRadiusFromPrimvar(GetShape(), id, delegate);
    }

    if (HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
        param->Interrupt();
        _UpdateVisibility(delegate, dirtyBits);
        AiNodeSetByte(GetShape(), str::visibility, _sharedData.visible ? AI_RAY_ALL : uint8_t{0});
    }

    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
        param->Interrupt();
        const auto* material = reinterpret_cast<const HdArnoldMaterial*>(
            delegate->GetRenderIndex().GetSprim(HdPrimTypeTokens->material, delegate->GetMaterialId(id)));
        if (material != nullptr) {
            AiNodeSetPtr(GetShape(), str::shader, material->GetSurfaceShader());
        } else {
            AiNodeSetPtr(GetShape(), str::shader, GetDelegate()->GetFallbackShader());
        }
    }

    if (*dirtyBits & HdChangeTracker::DirtyPrimvar) {
        param->Interrupt();
        for (const auto& primvar : delegate->GetPrimvarDescriptors(id, HdInterpolation::HdInterpolationConstant)) {
            HdArnoldSetConstantPrimvar(GetShape(), id, delegate, primvar);
        }

        auto convertToUniformPrimvar = [&](const HdPrimvarDescriptor& primvar) {
            if (primvar.name != HdTokens->points && primvar.name != HdTokens->widths) {
                HdArnoldSetUniformPrimvar(GetShape(), id, delegate, primvar);
            }
        };

        for (const auto& primvar : delegate->GetPrimvarDescriptors(id, HdInterpolation::HdInterpolationUniform)) {
            convertToUniformPrimvar(primvar);
        }

        for (const auto& primvar : delegate->GetPrimvarDescriptors(id, HdInterpolation::HdInterpolationVertex)) {
            // Per vertex attributes are uniform on points.
            convertToUniformPrimvar(primvar);
        }
    }

    SyncShape(*dirtyBits, delegate, param);

    *dirtyBits = HdChangeTracker::Clean;
}

HdDirtyBits HdArnoldPoints::_PropagateDirtyBits(HdDirtyBits bits) const { return bits & HdChangeTracker::AllDirty; }

void HdArnoldPoints::_InitRepr(const TfToken& reprToken, HdDirtyBits* dirtyBits)
{
    TF_UNUSED(reprToken);
    TF_UNUSED(dirtyBits);
}

PXR_NAMESPACE_CLOSE_SCOPE
