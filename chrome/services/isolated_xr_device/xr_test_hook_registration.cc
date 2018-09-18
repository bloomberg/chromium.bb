// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/isolated_xr_device/xr_test_hook_registration.h"
#include "chrome/services/isolated_xr_device/xr_test_hook_wrapper.h"
#include "device/vr/openvr/openvr_api_wrapper.h"

namespace device {

void XRTestHookRegistration::SetTestHook(
    device_test::mojom::XRTestHookPtr hook,
    device_test::mojom::XRTestHookRegistration::SetTestHookCallback callback) {
  // Create a new wrapper (or use null)
  std::unique_ptr<XRTestHookWrapper> wrapper =
      hook ? std::make_unique<XRTestHookWrapper>(hook.PassInterface())
           : nullptr;

  // Register the wrapper testhook with OpenVR.
  OpenVRWrapper::SetTestHook(wrapper.get());

  // Store the new wrapper, so we keep it alive.
  wrapper_ = std::move(wrapper);

  std::move(callback).Run();
}

XRTestHookRegistration::~XRTestHookRegistration() {
  // If we have an existing wrapper, and it is bound to a thread, post a message
  // to destroy it on that thread.
  if (wrapper_) {
    auto runner = wrapper_->GetBoundTaskRunner();
    runner->PostTask(FROM_HERE,
                     base::BindOnce(
                         [](std::unique_ptr<XRTestHookWrapper> wrapper) {
                           // Unset the testhook wrapper with OpenVR, so any
                           // future calls to OpenVR don't use it.
                           OpenVRWrapper::SetTestHook(nullptr);

                           // Destroy the test hook wrapper on this thread.
                         },
                         std::move(wrapper_)));
  }
}

XRTestHookRegistration::XRTestHookRegistration(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

}  // namespace device
