// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_device.h"

#include <math.h>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/math_constants.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace device {

namespace {

// Windows Mixed Reality doesn't give out display info until you start a
// presentation session and "Holographic Cameras" are added to the scene.
// However our mojo interface expects display info right away to support WebVR.
// We create a fake display info to use, then notify the client that the display
// info changed when we get real data.
mojom::VRDisplayInfoPtr CreateFakeVRDisplayInfo(device::mojom::XRDeviceId id) {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
  display_info->id = id;
  display_info->displayName = "Windows Mixed Reality";
  display_info->capabilities = mojom::VRDisplayCapabilities::New();
  display_info->capabilities->hasPosition = true;
  display_info->capabilities->hasExternalDisplay = true;
  display_info->capabilities->canPresent = true;
  display_info->webvr_default_framebuffer_scale = 1.0;
  display_info->webxr_default_framebuffer_scale = 1.0;

  display_info->leftEye = mojom::VREyeParameters::New();
  display_info->rightEye = mojom::VREyeParameters::New();
  mojom::VREyeParametersPtr& left_eye = display_info->leftEye;
  mojom::VREyeParametersPtr& right_eye = display_info->rightEye;

  left_eye->fieldOfView = mojom::VRFieldOfView::New(45, 45, 45, 45);
  right_eye->fieldOfView = mojom::VRFieldOfView::New(45, 45, 45, 45);

  constexpr float interpupillary_distance = 0.1f;  // 10cm
  left_eye->offset = {-interpupillary_distance * 0.5, 0, 0};
  right_eye->offset = {interpupillary_distance * 0.5, 0, 0};

  constexpr uint32_t width = 1024;
  constexpr uint32_t height = 1024;
  left_eye->renderWidth = width;
  left_eye->renderHeight = height;
  right_eye->renderWidth = left_eye->renderWidth;
  right_eye->renderHeight = left_eye->renderHeight;

  return display_info;
}

}  // namespace

MixedRealityDevice::MixedRealityDevice()
    : VRDeviceBase(device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID),
      gamepad_provider_factory_binding_(this),
      compositor_host_binding_(this),
      exclusive_controller_binding_(this),
      weak_ptr_factory_(this) {
  SetVRDisplayInfo(CreateFakeVRDisplayInfo(GetId()));
}

MixedRealityDevice::~MixedRealityDevice() {}

mojom::IsolatedXRGamepadProviderFactoryPtr
MixedRealityDevice::BindGamepadFactory() {
  mojom::IsolatedXRGamepadProviderFactoryPtr ret;
  gamepad_provider_factory_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
}

mojom::XRCompositorHostPtr MixedRealityDevice::BindCompositorHost() {
  mojom::XRCompositorHostPtr ret;
  compositor_host_binding_.Bind(mojo::MakeRequest(&ret));
  return ret;
}

void MixedRealityDevice::RequestSession(
    mojom::XRRuntimeSessionOptionsPtr options,
    mojom::XRRuntime::RequestSessionCallback callback) {
  // We don't yet support any sessions.
  std::move(callback).Run(nullptr, nullptr);
}

void MixedRealityDevice::GetIsolatedXRGamepadProvider(
    mojom::IsolatedXRGamepadProviderRequest provider_request) {
  // Not implemented yet.
}

void MixedRealityDevice::CreateImmersiveOverlay(
    mojom::ImmersiveOverlayRequest overlay_request) {
  // Not implemented yet.
}

// XRSessionController
void MixedRealityDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  DCHECK(false);
}

}  // namespace device
