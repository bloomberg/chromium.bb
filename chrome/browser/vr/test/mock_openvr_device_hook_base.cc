// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_openvr_device_hook_base.h"
#include "content/public/common/service_manager_connection.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

MockOpenVRDeviceHookBase::MockOpenVRDeviceHookBase() : binding_(this) {
  content::ServiceManagerConnection* connection =
      content::ServiceManagerConnection::GetForProcess();
  connection->GetConnector()->BindInterface(
      device::mojom::kVrIsolatedServiceName,
      mojo::MakeRequest(&test_hook_registration_));

  device_test::mojom::XRTestHookPtr client;
  binding_.Bind(mojo::MakeRequest(&client));

  mojo::ScopedAllowSyncCallForTesting scoped_allow_sync;
  test_hook_registration_->SetTestHook(std::move(client));
}

MockOpenVRDeviceHookBase::~MockOpenVRDeviceHookBase() {
  StopHooking();
}

void MockOpenVRDeviceHookBase::StopHooking() {
  // We don't call test_hook_registration_->SetTestHook(nullptr), since that
  // will potentially deadlock with reentrant or crossing synchronous mojo
  // calls.
  binding_.Close();
  test_hook_registration_ = nullptr;
}

void MockOpenVRDeviceHookBase::OnFrameSubmitted(
    device_test::mojom::SubmittedFrameDataPtr frame_data,
    device_test::mojom::XRTestHook::OnFrameSubmittedCallback callback) {
  std::move(callback).Run();
}

void MockOpenVRDeviceHookBase::WaitGetDeviceConfig(
    device_test::mojom::XRTestHook::WaitGetDeviceConfigCallback callback) {
  device_test::mojom::DeviceConfigPtr ret =
      device_test::mojom::DeviceConfig::New();
  ret->interpupillary_distance = 0.1f;
  ret->projection_left = device_test::mojom::ProjectionRaw::New(1, 1, 1, 1);
  ret->projection_right = device_test::mojom::ProjectionRaw::New(1, 1, 1, 1);
  std::move(callback).Run(std::move(ret));
}

void MockOpenVRDeviceHookBase::WaitGetPresentingPose(
    device_test::mojom::XRTestHook::WaitGetPresentingPoseCallback callback) {
  auto pose = device_test::mojom::PoseFrameData::New();
  pose->device_to_origin = gfx::Transform();
  std::move(callback).Run(std::move(pose));
}

void MockOpenVRDeviceHookBase::WaitGetMagicWindowPose(
    device_test::mojom::XRTestHook::WaitGetMagicWindowPoseCallback callback) {
  auto pose = device_test::mojom::PoseFrameData::New();
  pose->device_to_origin = gfx::Transform();
  std::move(callback).Run(std::move(pose));
}
