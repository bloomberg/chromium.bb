// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_input_helper.h"

#include <SpatialInteractionManagerInterop.h>
#include <windows.perception.h>
#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>

#include <wrl.h>

#include <unordered_map>
#include <vector>

#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

// We want to differentiate from gfx::Members, so we're not going to explicitly
// use anything from Windows::Foundation::Numerics
namespace WFN = ABI::Windows::Foundation::Numerics;

using Handedness =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceHandedness;
using SourceKind =
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceKind;

using ABI::Windows::Foundation::IReference;
using ABI::Windows::Foundation::Collections::IVectorView;
using ABI::Windows::Perception::IPerceptionTimestamp;
using ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource2;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource3;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation2;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation3;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceProperties;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceState2;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose2;
using ABI::Windows::UI::Input::Spatial::SpatialInteractionSourceState;
using Microsoft::WRL::ComPtr;

namespace {
gfx::Transform CreateTransform(WFN::Vector3 position,
                               WFN::Quaternion rotation) {
  gfx::DecomposedTransform decomposed_transform;
  decomposed_transform.translate[0] = position.X;
  decomposed_transform.translate[1] = position.Y;
  decomposed_transform.translate[2] = position.Z;

  decomposed_transform.quaternion =
      gfx::Quaternion(rotation.X, rotation.Y, rotation.Z, rotation.W);
  return gfx::ComposeTransform(decomposed_transform);
}

bool TryGetGripFromLocation(
    ComPtr<ISpatialInteractionSourceLocation> location_in_origin,
    gfx::Transform* origin_from_grip) {
  DCHECK(origin_from_grip);
  *origin_from_grip = gfx::Transform();

  if (!location_in_origin)
    return false;

  ComPtr<IReference<WFN::Vector3>> pos_ref;

  if (FAILED(location_in_origin->get_Position(&pos_ref)) || !pos_ref)
    return false;

  WFN::Vector3 pos;
  if (FAILED(pos_ref->get_Value(&pos)))
    return false;

  ComPtr<ISpatialInteractionSourceLocation2> location_in_origin2;
  if (FAILED(location_in_origin.As(&location_in_origin2)))
    return false;

  ComPtr<IReference<WFN::Quaternion>> quat_ref;
  if (FAILED(location_in_origin2->get_Orientation(&quat_ref)) || !quat_ref)
    return false;

  WFN::Quaternion quat;
  if (FAILED(quat_ref->get_Value(&quat)))
    return false;

  *origin_from_grip = CreateTransform(pos, quat);
  return true;
}

bool TryGetPointerOffsetFromLocation(
    ComPtr<ISpatialInteractionSourceLocation> location_in_origin,
    gfx::Transform origin_from_grip,
    gfx::Transform* grip_from_pointer) {
  DCHECK(grip_from_pointer);
  *grip_from_pointer = gfx::Transform();

  if (!location_in_origin)
    return false;

  // We can get the pointer position, but we'll need to transform it to an
  // offset from the grip position.  If we can't get an inverse of that,
  // then go ahead and bail early.
  gfx::Transform grip_from_origin;
  if (!origin_from_grip.GetInverse(&grip_from_origin))
    return false;

  ComPtr<ISpatialInteractionSourceLocation3> location_in_origin3;
  if (FAILED(location_in_origin.As(&location_in_origin3)))
    return false;

  ComPtr<ISpatialPointerInteractionSourcePose> pointer_pose;
  if (FAILED(location_in_origin3->get_SourcePointerPose(&pointer_pose)) ||
      !pointer_pose)
    return false;

  WFN::Vector3 pos;
  if (FAILED(pointer_pose->get_Position(&pos)))
    return false;

  ComPtr<ISpatialPointerInteractionSourcePose2> pose2;
  if (FAILED(pointer_pose.As(&pose2)))
    return false;

  WFN::Quaternion rot;
  if (FAILED(pose2->get_Orientation(&rot)))
    return false;

  gfx::Transform origin_from_pointer = CreateTransform(pos, rot);
  *grip_from_pointer = (grip_from_origin * origin_from_pointer);
  return true;
}
}  // namespace

MixedRealityInputHelper::MixedRealityInputHelper(HWND hwnd) : hwnd_(hwnd) {}

MixedRealityInputHelper::~MixedRealityInputHelper() {}

bool MixedRealityInputHelper::EnsureSpatialInteractionManager() {
  if (spatial_interaction_manager_)
    return true;

  if (!hwnd_)
    return false;

  ComPtr<ISpatialInteractionManagerInterop> spatial_interaction_interop;
  base::win::ScopedHString spatial_interaction_interop_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_UI_Input_Spatial_SpatialInteractionManager);
  HRESULT hr = base::win::RoGetActivationFactory(
      spatial_interaction_interop_string.get(),
      IID_PPV_ARGS(&spatial_interaction_interop));
  if (FAILED(hr))
    return false;

  hr = spatial_interaction_interop->GetForWindow(
      hwnd_, IID_PPV_ARGS(&spatial_interaction_manager_));
  return SUCCEEDED(hr);
}

std::vector<mojom::XRInputSourceStatePtr>
MixedRealityInputHelper::GetInputState(ComPtr<ISpatialCoordinateSystem> origin,
                                       ComPtr<IPerceptionTimestamp> timestamp) {
  std::vector<mojom::XRInputSourceStatePtr> input_states;

  if (!timestamp || !origin || !EnsureSpatialInteractionManager())
    return input_states;

  ComPtr<IVectorView<SpatialInteractionSourceState*>> source_states;
  if (FAILED(spatial_interaction_manager_->GetDetectedSourcesAtTimestamp(
          timestamp.Get(), &source_states)))
    return input_states;

  unsigned int size;
  if (FAILED(source_states->get_Size(&size)))
    return input_states;

  for (unsigned int i = 0; i < size; i++) {
    ComPtr<ISpatialInteractionSourceState> state;
    if (FAILED(source_states->GetAt(i, &state)))
      continue;

    // Get the source and query for all of the state that we send first
    ComPtr<ISpatialInteractionSource> source;
    if (FAILED(state->get_Source(&source)))
      continue;

    SourceKind source_kind;
    if (FAILED(source->get_Kind(&source_kind)) ||
        source_kind != SourceKind::SpatialInteractionSourceKind_Controller)
      continue;

    uint32_t id;
    if (FAILED(source->get_Id(&id)))
      continue;

    ComPtr<ISpatialInteractionSourceState2> state2;
    if (FAILED(state.As(&state2)))
      continue;

    boolean pressed = false;
    if (FAILED(state2->get_IsSelectPressed(&pressed)))
      continue;

    ComPtr<ISpatialInteractionSourceProperties> props;
    if (FAILED(state->get_Properties(&props)))
      continue;

    ComPtr<ISpatialInteractionSourceLocation> location_in_origin;
    if (FAILED(props->TryGetLocation(origin.Get(), &location_in_origin)) ||
        !location_in_origin)
      continue;

    gfx::Transform origin_from_grip;
    if (!TryGetGripFromLocation(location_in_origin, &origin_from_grip))
      continue;

    ComPtr<ISpatialInteractionSource2> source2;
    boolean pointingSupported = false;
    if (FAILED(source.As(&source2)) ||
        FAILED(source2->get_IsPointingSupported(&pointingSupported)) ||
        !pointingSupported)
      continue;

    gfx::Transform grip_from_pointer;
    if (!TryGetPointerOffsetFromLocation(location_in_origin, origin_from_grip,
                                         &grip_from_pointer))
      continue;

    // Now that we've queried for all of the state we're going to send,
    // build the object to send it.
    device::mojom::XRInputSourceStatePtr source_state =
        device::mojom::XRInputSourceState::New();

    // Hands may not have the same id especially if they are lost but since we
    // are only tracking controllers, this id should be consistent.
    source_state->source_id = id;

    source_state->primary_input_pressed = pressed;
    source_state->primary_input_clicked =
        !pressed && controller_pressed_state_[id];
    controller_pressed_state_[id] = pressed;

    source_state->grip = origin_from_grip;

    device::mojom::XRInputSourceDescriptionPtr description =
        device::mojom::XRInputSourceDescription::New();

    // It's a fully 6DoF handheld pointing device
    description->emulated_position = false;
    description->target_ray_mode = device::mojom::XRTargetRayMode::POINTING;

    description->pointer_offset = grip_from_pointer;

    Handedness handedness;
    ComPtr<ISpatialInteractionSource3> source3;
    if (SUCCEEDED(source.As(&source3) &&
                  SUCCEEDED(source3->get_Handedness(&handedness)))) {
      switch (handedness) {
        case Handedness::SpatialInteractionSourceHandedness_Left:
          description->handedness = device::mojom::XRHandedness::LEFT;
          break;
        case Handedness::SpatialInteractionSourceHandedness_Right:
          description->handedness = device::mojom::XRHandedness::RIGHT;
          break;
        default:
          description->handedness = device::mojom::XRHandedness::NONE;
          break;
      }
    } else {
      description->handedness = device::mojom::XRHandedness::NONE;
    }

    source_state->description = std::move(description);
    input_states.push_back(std::move(source_state));
  }

  return input_states;
}

}  // namespace device
