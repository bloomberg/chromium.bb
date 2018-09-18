// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_MOCK_OPENVR_DEVICE_HOOK_BASE_H_
#define CHROME_BROWSER_VR_TEST_MOCK_OPENVR_DEVICE_HOOK_BASE_H_

#include "device/vr/public/mojom/browser_test_interfaces.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

class MockOpenVRDeviceHookBase : public device_test::mojom::XRTestHook {
 public:
  MockOpenVRDeviceHookBase();
  ~MockOpenVRDeviceHookBase() override;

  // device_test::mojom::XRTestHook
  void OnFrameSubmitted(device_test::mojom::SubmittedFrameDataPtr frame_data,
                        device_test::mojom::XRTestHook::OnFrameSubmittedCallback
                            callback) override;
  void WaitGetDeviceConfig(
      device_test::mojom::XRTestHook::WaitGetDeviceConfigCallback callback)
      override;
  void WaitGetPresentingPose(
      device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback)
      override;
  void WaitGetMagicWindowPose(
      device_test::mojom::XRTestHook::WaitGetMagicWindowPoseCallback callback)
      override;

  void StopHooking();

 private:
  mojo::Binding<device_test::mojom::XRTestHook> binding_;
  device_test::mojom::XRTestHookRegistrationPtr test_hook_registration_;
};

#endif  // CHROME_BROWSER_VR_TEST_MOCK_OPENVR_DEVICE_HOOK_BASE_H_
