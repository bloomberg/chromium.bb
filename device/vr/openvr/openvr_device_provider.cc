// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_device_provider.h"

#include "device/gamepad/gamepad_data_fetcher_manager.h"
#include "device/vr/openvr/openvr_device.h"
#include "device/vr/openvr/openvr_gamepad_data_fetcher.h"
#include "third_party/openvr/src/headers/openvr.h"

namespace device {

OpenVRDeviceProvider::OpenVRDeviceProvider()
    : initialized_(false), vr_system_(nullptr) {}

OpenVRDeviceProvider::~OpenVRDeviceProvider() {
  device::GamepadDataFetcherManager::GetInstance()->RemoveSourceFactory(
      device::GAMEPAD_SOURCE_OPENVR);
  device_ = nullptr;
  vr::VR_Shutdown();
}

void OpenVRDeviceProvider::GetDevices(std::vector<VRDevice*>* devices) {
  if (initialized_) {
    if (!device_) {
      device_ = std::make_unique<OpenVRDevice>(vr_system_);
      GamepadDataFetcherManager::GetInstance()->AddFactory(
          new OpenVRGamepadDataFetcher::Factory(device_->GetId(), vr_system_));
    }

    if (device_) {
      devices->push_back(device_.get());
    }
  }
}

void OpenVRDeviceProvider::Initialize() {
  if (!initialized_ && vr::VR_IsRuntimeInstalled() && vr::VR_IsHmdPresent()) {
    vr::EVRInitError init_error = vr::VRInitError_None;
    vr_system_ =
        vr::VR_Init(&init_error, vr::EVRApplicationType::VRApplication_Scene);

    if (init_error != vr::VRInitError_None) {
      LOG(ERROR) << vr::VR_GetVRInitErrorAsEnglishDescription(init_error);
      vr_system_ = nullptr;
      return;
    }

    initialized_ = true;
  }
}

}  // namespace device
