// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chromeos/services/device_sync/fake_device_sync.h"

#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"

namespace chromeos {

namespace device_sync {

FakeDeviceSync::FakeDeviceSync() : DeviceSyncBase() {}

FakeDeviceSync::~FakeDeviceSync() = default;

void FakeDeviceSync::InvokePendingGetLocalDeviceMetadataCallback(
    const base::Optional<multidevice::RemoteDevice>& local_device_metadata) {
  std::move(get_local_device_metadata_callback_queue_.front())
      .Run(local_device_metadata);
  get_local_device_metadata_callback_queue_.pop();
}

void FakeDeviceSync::InvokePendingGetSyncedDevicesCallback(
    const base::Optional<std::vector<multidevice::RemoteDevice>>&
        remote_devices) {
  std::move(get_synced_devices_callback_queue_.front()).Run(remote_devices);
  get_synced_devices_callback_queue_.pop();
}

void FakeDeviceSync::InvokePendingSetSoftwareFeatureStateCallback(
    mojom::NetworkRequestResult result_code) {
  std::move(set_software_feature_state_callback_queue_.front())
      .Run(result_code);
  set_software_feature_state_callback_queue_.pop();
}

void FakeDeviceSync::InvokePendingSetFeatureStatusCallback(
    mojom::NetworkRequestResult result_code) {
  std::move(set_feature_status_callback_queue_.front()).Run(result_code);
  set_feature_status_callback_queue_.pop();
}

void FakeDeviceSync::InvokePendingFindEligibleDevicesCallback(
    mojom::NetworkRequestResult result_code,
    mojom::FindEligibleDevicesResponsePtr find_eligible_devices_response_ptr) {
  std::move(find_eligible_devices_callback_queue_.front())
      .Run(result_code, std::move(find_eligible_devices_response_ptr));
  find_eligible_devices_callback_queue_.pop();
}

void FakeDeviceSync::InvokePendingNotifyDevicesCallback(
    mojom::NetworkRequestResult result_code) {
  std::move(notify_devices_callback_queue_.front()).Run(result_code);
  notify_devices_callback_queue_.pop();
}

void FakeDeviceSync::InvokePendingGetDevicesActivityStatusCallback(
    mojom::NetworkRequestResult result_code,
    base::Optional<std::vector<mojom::DeviceActivityStatusPtr>>
        get_devices_activity_status_response) {
  std::move(get_devices_activity_status_callback_queue_.front())
      .Run(result_code, std::move(get_devices_activity_status_response));
  get_devices_activity_status_callback_queue_.pop();
}

void FakeDeviceSync::InvokePendingGetDebugInfoCallback(
    mojom::DebugInfoPtr debug_info_ptr) {
  std::move(get_debug_info_callback_queue_.front())
      .Run(std::move(debug_info_ptr));
  get_debug_info_callback_queue_.pop();
}

void FakeDeviceSync::ForceEnrollmentNow(ForceEnrollmentNowCallback callback) {
  std::move(callback).Run(force_enrollment_now_completed_success_);
}

void FakeDeviceSync::ForceSyncNow(ForceSyncNowCallback callback) {
  std::move(callback).Run(force_sync_now_completed_success_);
}

void FakeDeviceSync::GetLocalDeviceMetadata(
    GetLocalDeviceMetadataCallback callback) {
  get_local_device_metadata_callback_queue_.push(std::move(callback));
}

void FakeDeviceSync::GetSyncedDevices(GetSyncedDevicesCallback callback) {
  get_synced_devices_callback_queue_.push(std::move(callback));
}

void FakeDeviceSync::SetSoftwareFeatureState(
    const std::string& device_public_key,
    multidevice::SoftwareFeature software_feature,
    bool enabled,
    bool is_exclusive,
    SetSoftwareFeatureStateCallback callback) {
  set_software_feature_state_callback_queue_.push(std::move(callback));
}

void FakeDeviceSync::SetFeatureStatus(const std::string& device_instance_id,
                                      multidevice::SoftwareFeature feature,
                                      FeatureStatusChange status_change,
                                      SetFeatureStatusCallback callback) {
  set_feature_status_callback_queue_.push(std::move(callback));
}

void FakeDeviceSync::FindEligibleDevices(
    multidevice::SoftwareFeature software_feature,
    FindEligibleDevicesCallback callback) {
  find_eligible_devices_callback_queue_.push(std::move(callback));
}

void FakeDeviceSync::NotifyDevices(
    const std::vector<std::string>& device_instance_ids,
    cryptauthv2::TargetService target_service,
    multidevice::SoftwareFeature feature,
    NotifyDevicesCallback callback) {
  notify_devices_callback_queue_.push(std::move(callback));
}

void FakeDeviceSync::GetDebugInfo(GetDebugInfoCallback callback) {
  get_debug_info_callback_queue_.push(std::move(callback));
}

void FakeDeviceSync::GetDevicesActivityStatus(
    GetDevicesActivityStatusCallback callback) {
  get_devices_activity_status_callback_queue_.push(std::move(callback));
}

}  // namespace device_sync

}  // namespace chromeos
