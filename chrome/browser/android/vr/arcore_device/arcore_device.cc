// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_device.h"

#include <jni.h>
#include "base/bind.h"
#include "base/numerics/math_constants.h"
#include "base/optional.h"
#include "base/task_scheduler/post_task.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl.h"
#include "chrome/browser/android/vr/arcore_device/arcore_gl_thread.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"

using base::android::JavaRef;

namespace {
constexpr float kDegreesPerRadian = 180.0f / base::kPiFloat;
}  // namespace

namespace device {

namespace {

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(uint32_t device_id) {
  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();
  device->index = device_id;
  device->displayName = "ARCore VR Device";
  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->hasPosition = true;
  device->capabilities->hasExternalDisplay = false;
  device->capabilities->canPresent = false;
  device->capabilities->can_provide_pass_through_images = true;
  device->leftEye = mojom::VREyeParameters::New();
  device->rightEye = nullptr;
  mojom::VREyeParametersPtr& left_eye = device->leftEye;
  left_eye->fieldOfView = mojom::VRFieldOfView::New();
  // TODO(lincolnfrog): get these values for real (see gvr device).
  uint width = 1080;
  uint height = 1795;
  double fov_x = 1437.387;
  double fov_y = 1438.074;
  // TODO(lincolnfrog): get real camera intrinsics.
  float horizontal_degrees = atan(width / (2.0 * fov_x)) * kDegreesPerRadian;
  float vertical_degrees = atan(height / (2.0 * fov_y)) * kDegreesPerRadian;
  left_eye->fieldOfView->leftDegrees = horizontal_degrees;
  left_eye->fieldOfView->rightDegrees = horizontal_degrees;
  left_eye->fieldOfView->upDegrees = vertical_degrees;
  left_eye->fieldOfView->downDegrees = vertical_degrees;
  left_eye->offset = {0.0f, 0.0f, 0.0f};
  left_eye->renderWidth = width;
  left_eye->renderHeight = height;
  return device;
}

}  // namespace

ARCoreDevice::ARCoreDevice()
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      mailbox_bridge_(std::make_unique<vr::MailboxToSurfaceBridge>()),
      weak_ptr_factory_(this) {
  SetVRDisplayInfo(CreateVRDisplayInfo(GetId()));

  // TODO(https://crbug.com/836524) clean up usage of mailbox bridge
  // and extract the methods in this class that interact with ARCore API
  // into a separate class that implements the ARCoreDriverAPI interface.
  mailbox_bridge_->CreateUnboundContextProvider(
      base::BindOnce(&ARCoreDevice::OnMailboxBridgeReady, GetWeakPtr()));
}

ARCoreDevice::~ARCoreDevice() {
  if (arcore_gl_thread_)
    arcore_gl_thread_->Stop();
}

void ARCoreDevice::PauseTracking() {
  DCHECK(IsOnMainThread());

  if (is_paused_)
    return;

  is_paused_ = true;

  if (!is_arcore_gl_thread_initialized_)
    return;

  PostTaskToGlThread(base::BindOnce(
      &ARCoreGl::Pause, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr()));
}

void ARCoreDevice::ResumeTracking() {
  DCHECK(IsOnMainThread());

  if (!is_paused_)
    return;

  is_paused_ = false;

  if (!is_arcore_gl_thread_initialized_)
    return;

  PostTaskToGlThread(base::BindOnce(
      &ARCoreGl::Resume, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr()));
}

void ARCoreDevice::OnMailboxBridgeReady() {
  DCHECK(IsOnMainThread());
  DCHECK(!arcore_gl_thread_);
  // MailboxToSurfaceBridge's destructor's call to DestroyContext must
  // happen on the GL thread, so transferring it to that thread is appropriate.
  // TODO(https://crbug.com/836553): use same GL thread as GVR.
  arcore_gl_thread_ = std::make_unique<ARCoreGlThread>(
      std::move(mailbox_bridge_),
      CreateMainThreadCallback(base::BindOnce(
          &ARCoreDevice::OnARCoreGlThreadInitialized, GetWeakPtr())));
  arcore_gl_thread_->Start();
}

void ARCoreDevice::OnARCoreGlThreadInitialized(bool success) {
  DCHECK(IsOnMainThread());

  if (!success) {
    DLOG(ERROR) << "Failed to initialize ARCoreDevice/GL system!";
    return;
  }

  is_arcore_gl_thread_initialized_ = true;

  if (!is_paused_) {
    PostTaskToGlThread(base::BindOnce(
        &ARCoreGl::Resume, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr()));
  }
}

void ARCoreDevice::RequestSession(
    VRDisplayImpl* display,
    mojom::VRDisplayHost::RequestSessionCallback callback) {
  // TODO(https://crbug.com/837116): Hook up ARCoreDevice to ARCore library
  std::move(callback).Run(true);
}

bool ARCoreDevice::ShouldPauseAndResumeOnFocusChange() {
  return true;
}

void ARCoreDevice::OnMagicWindowFrameDataRequest(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    mojom::VRMagicWindowProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsOnMainThread());

  // TODO(ijamardo): Do we need to queue requests to avoid breaking
  // applications?
  // TODO(https://crbug.com/837944): Ensure is_arcore_gl_thread_initialized_
  // is always true by blocking requestDevice()'s callback until it is true
  if (is_paused_ || !is_arcore_gl_thread_initialized_) {
    std::move(callback).Run(nullptr);
    return;
  }

  PostTaskToGlThread(base::BindOnce(
      &ARCoreGl::ProduceFrame, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr(),
      frame_size, display_rotation,
      CreateMainThreadCallback(std::move(callback))));
}

void ARCoreDevice::RequestHitTest(
    mojom::XRRayPtr ray,
    mojom::VRMagicWindowProvider::RequestHitTestCallback callback) {
  DCHECK(IsOnMainThread());

  PostTaskToGlThread(base::BindOnce(
      &ARCoreGl::RequestHitTest, arcore_gl_thread_->GetARCoreGl()->GetWeakPtr(),
      std::move(ray), CreateMainThreadCallback(std::move(callback))));
}

void ARCoreDevice::PostTaskToGlThread(base::OnceClosure task) {
  DCHECK(IsOnMainThread());
  arcore_gl_thread_->GetARCoreGl()->GetGlThreadTaskRunner()->PostTask(
      FROM_HERE, std::move(task));
}

bool ARCoreDevice::IsOnMainThread() {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

}  // namespace device
