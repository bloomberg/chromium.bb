// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_device_provider.h"

#include "base/metrics/histogram_macros.h"
#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/vr/openvr/openvr_device.h"
#include "device/vr/openvr/openvr_gamepad_data_fetcher.h"
#include "device/vr/openvr/test/test_hook.h"
#include "third_party/openvr/src/headers/openvr.h"

#include "base/command_line.h"

namespace device {

void OpenVRDeviceProvider::RecordRuntimeAvailability() {
  XrRuntimeAvailable runtime = XrRuntimeAvailable::NONE;
  if (vr::VR_IsRuntimeInstalled())
    runtime = XrRuntimeAvailable::OPENVR;
  UMA_HISTOGRAM_ENUMERATION("XR.RuntimeAvailable", runtime,
                            XrRuntimeAvailable::COUNT);
}

OpenVRDeviceProvider::OpenVRDeviceProvider() = default;

OpenVRDeviceProvider::~OpenVRDeviceProvider() {
  device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      device::GAMEPAD_SOURCE_OPENVR);
  // We must shutdown device_ and set it to null before calling VR_Shutdown,
  // because VR_Shutdown will unload OpenVR's dll, and device_ (or its render
  // loop) are potentially still using it.
  if (device_) {
    device_->Shutdown();
    device_ = nullptr;
  }

  if (test_hook_registration_s) {
    test_hook_registration_s->SetTestHook(nullptr);
  }

  vr::VR_Shutdown();
  test_hook_registration_s = nullptr;
}

void OpenVRDeviceProvider::Initialize(
    base::RepeatingCallback<void(unsigned int,
                                 mojom::VRDisplayInfoPtr,
                                 mojom::XRRuntimePtr)> add_device_callback,
    base::RepeatingCallback<void(unsigned int)> remove_device_callback,
    base::OnceClosure initialization_complete) {
  CreateDevice();
  if (device_)
    add_device_callback.Run(device_->GetId(), device_->GetVRDisplayInfo(),
                            device_->BindXRRuntimePtr());
  initialized_ = true;
  std::move(initialization_complete).Run();
}

void OpenVRDeviceProvider::SetTestHook(OpenVRTestHook* test_hook) {
  test_hook_s = test_hook;
  if (test_hook_registration_s) {
    test_hook_registration_s->SetTestHook(test_hook_s);
  }
}

OpenVRTestHook* OpenVRDeviceProvider::test_hook_s = nullptr;
TestHookRegistration* OpenVRDeviceProvider::test_hook_registration_s = nullptr;

void OpenVRDeviceProvider::CreateDevice() {
  if (!vr::VR_IsRuntimeInstalled() || !vr::VR_IsHmdPresent())
    return;

  vr::EVRInitError init_error = vr::VRInitError_None;
  vr::IVRSystem* vr_system =
      vr::VR_Init(&init_error, vr::EVRApplicationType::VRApplication_Scene);

  if (test_hook_s) {
    // Allow our mock implementation of OpenVR to be controlled by tests.
    // Note that SetTestHook must be called before CreateDevice, or
    // test_hook_registration_s will remain null.  This is a good pattern for
    // tests anyway, since the alternative is we start mocking part-way through
    // using the device, and end up with race conditions for when we started
    // controlling things.
    vr::EVRInitError eError;
    test_hook_registration_s = reinterpret_cast<TestHookRegistration*>(
        vr::VR_GetGenericInterface(kChromeOpenVRTestHookAPI, &eError));
    if (test_hook_registration_s) {
      test_hook_registration_s->SetTestHook(test_hook_s);
    }
  }

  if (init_error != vr::VRInitError_None) {
    LOG(ERROR) << vr::VR_GetVRInitErrorAsEnglishDescription(init_error);
    return;
  }
  device_ = std::make_unique<OpenVRDevice>(vr_system);
  GamepadDataFetcherManager::GetInstance()->AddFactory(
      new OpenVRGamepadDataFetcher::Factory(device_->GetId(), vr_system));
}

bool OpenVRDeviceProvider::Initialized() {
  return initialized_;
}

}  // namespace device
