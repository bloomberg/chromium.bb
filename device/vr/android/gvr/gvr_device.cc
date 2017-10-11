// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_device.h"

#include <math.h>
#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"
#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"
#include "device/vr/android/gvr/gvr_device_provider.h"
#include "device/vr/vr_display_impl.h"
#include "jni/NonPresentingGvrContext_jni.h"
#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

using base::android::JavaRef;

namespace device {

namespace {

// Default downscale factor for computing the recommended WebVR
// renderWidth/Height from the 1:1 pixel mapped size. Using a rather
// aggressive downscale due to the high overhead of copying pixels
// twice before handing off to GVR. For comparison, the polyfill
// uses approximately 0.55 on a Pixel XL.
static constexpr float kWebVrRecommendedResolutionScale = 0.5;

gfx::Size GetRecommendedWebVrSize(gvr::GvrApi* gvr_api) {
  // Pick a reasonable default size for the WebVR transfer surface
  // based on a downscaled 1:1 render resolution. This size will also
  // be reported to the client via CreateVRDisplayInfo as the
  // client-recommended renderWidth/renderHeight and for the GVR
  // framebuffer. If the client chooses a different size or resizes it
  // while presenting, we'll resize the transfer surface and GVR
  // framebuffer to match.
  gvr::Sizei render_target_size =
      gvr_api->GetMaximumEffectiveRenderTargetSize();

  gfx::Size webvr_size(
      render_target_size.width * kWebVrRecommendedResolutionScale,
      render_target_size.height * kWebVrRecommendedResolutionScale);

  // Ensure that the width is an even number so that the eyes each
  // get the same size, the recommended renderWidth is per eye
  // and the client will use the sum of the left and right width.
  //
  // TODO(klausw,crbug.com/699350): should we round the recommended
  // size to a multiple of 2^N pixels to be friendlier to the GPU? The
  // exact size doesn't matter, and it might be more efficient.
  webvr_size.set_width(webvr_size.width() & ~1);
  return webvr_size;
}

mojom::VREyeParametersPtr CreateEyeParamater(
    gvr::GvrApi* gvr_api,
    gvr::Eye eye,
    const gvr::BufferViewportList& buffers,
    const gfx::Size& recommended_size) {
  mojom::VREyeParametersPtr eye_params = mojom::VREyeParameters::New();
  eye_params->fieldOfView = mojom::VRFieldOfView::New();
  eye_params->offset.resize(3);
  eye_params->renderWidth = recommended_size.width() / 2;
  eye_params->renderHeight = recommended_size.height();

  gvr::BufferViewport eye_viewport = gvr_api->CreateBufferViewport();
  buffers.GetBufferViewport(eye, &eye_viewport);
  gvr::Rectf eye_fov = eye_viewport.GetSourceFov();
  eye_params->fieldOfView->upDegrees = eye_fov.top;
  eye_params->fieldOfView->downDegrees = eye_fov.bottom;
  eye_params->fieldOfView->leftDegrees = eye_fov.left;
  eye_params->fieldOfView->rightDegrees = eye_fov.right;

  gvr::Mat4f eye_mat = gvr_api->GetEyeFromHeadMatrix(eye);
  eye_params->offset[0] = -eye_mat.m[0][3];
  eye_params->offset[1] = -eye_mat.m[1][3];
  eye_params->offset[2] = -eye_mat.m[2][3];
  return eye_params;
}

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(gvr::GvrApi* gvr_api,
                                            gfx::Size recommended_size,
                                            uint32_t device_id) {
  TRACE_EVENT0("input", "GvrDelegate::CreateVRDisplayInfo");

  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();

  device->index = device_id;

  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->hasPosition = false;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = true;

  std::string vendor = gvr_api->GetViewerVendor();
  std::string model = gvr_api->GetViewerModel();
  device->displayName = vendor + " " + model;

  gvr::BufferViewportList gvr_buffer_viewports =
      gvr_api->CreateEmptyBufferViewportList();
  gvr_buffer_viewports.SetToRecommendedBufferViewports();

  device->leftEye = CreateEyeParamater(gvr_api, GVR_LEFT_EYE,
                                       gvr_buffer_viewports, recommended_size);
  device->rightEye = CreateEyeParamater(gvr_api, GVR_RIGHT_EYE,
                                        gvr_buffer_viewports, recommended_size);
  return device;
}

}  // namespace

std::unique_ptr<GvrDevice> GvrDevice::Create() {
  std::unique_ptr<GvrDevice> device = base::WrapUnique(new GvrDevice());
  if (!device->gvr_api_)
    return nullptr;
  return device;
}

GvrDevice::GvrDevice() : VRDevice(), weak_ptr_factory_(this) {
  GetGvrDelegateProvider();
  JNIEnv* env = base::android::AttachCurrentThread();
  non_presenting_context_.Reset(
      Java_NonPresentingGvrContext_create(env, reinterpret_cast<jlong>(this)));
  if (!non_presenting_context_.obj())
    return;
  jlong context = Java_NonPresentingGvrContext_getNativeGvrContext(
      env, non_presenting_context_);
  gvr_api_ = gvr::GvrApi::WrapNonOwned(reinterpret_cast<gvr_context*>(context));
  display_info_ = CreateVRDisplayInfo(
      gvr_api_.get(), GetRecommendedWebVrSize(gvr_api_.get()), id());
}

GvrDevice::~GvrDevice() {
  if (!non_presenting_context_.obj())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_NonPresentingGvrContext_shutdown(env, non_presenting_context_);
}

mojom::VRDisplayInfoPtr GvrDevice::GetVRDisplayInfo() {
  return display_info_.Clone();
}

void GvrDevice::RequestPresent(
    VRDisplayImpl* display,
    mojom::VRSubmitFrameClientPtr submit_client,
    mojom::VRPresentationProviderRequest request,
    mojom::VRDisplayHost::RequestPresentCallback callback) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider)
    return std::move(callback).Run(false);

  // RequestWebVRPresent is async as we may trigger a DON (Device ON) flow that
  // pauses Chrome.
  delegate_provider->RequestWebVRPresent(
      std::move(submit_client), std::move(request), GetVRDisplayInfo(),
      base::Bind(&GvrDevice::OnRequestPresentResult,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&callback),
                 base::Unretained(display)));
}

void GvrDevice::OnRequestPresentResult(
    mojom::VRDisplayHost::RequestPresentCallback callback,
    VRDisplayImpl* display,
    bool result) {
  if (result)
    SetPresentingDisplay(display);
  std::move(callback).Run(result);
}

void GvrDevice::ExitPresent() {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (delegate_provider)
    delegate_provider->ExitWebVRPresent();
  OnExitPresent();
}

void GvrDevice::GetPose(
    mojom::VRMagicWindowProvider::GetPoseCallback callback) {
  std::move(callback).Run(
      GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_.get(), nullptr));
}

void GvrDevice::OnListeningForActivateChanged(bool listening) {
  GvrDelegateProvider* delegate_provider = GetGvrDelegateProvider();
  if (!delegate_provider)
    return;
  delegate_provider->OnListeningForActivateChanged(listening);
}

void GvrDevice::PauseTracking() {
  gvr_api_->PauseTracking();
}

void GvrDevice::ResumeTracking() {
  gvr_api_->ResumeTracking();
}

GvrDelegateProvider* GvrDevice::GetGvrDelegateProvider() {
  // GvrDelegateProviderFactory::Create() may fail transiently, so every time we
  // try to get it, set the device ID.
  GvrDelegateProvider* delegate_provider = GvrDelegateProviderFactory::Create();
  if (delegate_provider)
    delegate_provider->SetDeviceId(id());
  return delegate_provider;
}

void GvrDevice::OnDIPScaleChanged(JNIEnv* env, const JavaRef<jobject>& obj) {
  display_info_ = CreateVRDisplayInfo(
      gvr_api_.get(), GetRecommendedWebVrSize(gvr_api_.get()), id());
  OnChanged();
}

}  // namespace device
