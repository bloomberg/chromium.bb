// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/device_reenroller.h"

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "components/cryptauth/gcm_device_info_provider.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

// The number of minutes to wait before retrying a failed re-enrollment or
// device sync attempt.
const int kNumMinutesBetweenRetries = 5;

std::vector<cryptauth::SoftwareFeature>
ComputeSupportedSoftwareFeaturesSortedDedupedListFromGcmDeviceInfo(
    const cryptauth::GcmDeviceInfo& gcm_device_info) {
  base::flat_set<cryptauth::SoftwareFeature> sorted_and_deduped_set;
  for (int i = 0; i < gcm_device_info.supported_software_features_size(); ++i) {
    sorted_and_deduped_set.insert(
        gcm_device_info.supported_software_features(i));
  }
  return std::vector<cryptauth::SoftwareFeature>(sorted_and_deduped_set.begin(),
                                                 sorted_and_deduped_set.end());
}

std::vector<cryptauth::SoftwareFeature>
ComputeSupportedSoftwareFeaturesSortedDedupedListFromLocalDeviceMetadata(
    const cryptauth::RemoteDeviceRef& local_device_metadata) {
  base::flat_set<cryptauth::SoftwareFeature> sorted_and_deduped_set;
  for (int i = cryptauth::SoftwareFeature_MIN;
       i <= cryptauth::SoftwareFeature_MAX; ++i) {
    cryptauth::SoftwareFeature feature =
        static_cast<cryptauth::SoftwareFeature>(i);
    if (local_device_metadata.GetSoftwareFeatureState(feature) !=
        cryptauth::SoftwareFeatureState::kNotSupported) {
      sorted_and_deduped_set.insert(feature);
    }
  }
  return std::vector<cryptauth::SoftwareFeature>(sorted_and_deduped_set.begin(),
                                                 sorted_and_deduped_set.end());
}

}  // namespace

// static
DeviceReenroller::Factory* DeviceReenroller::Factory::test_factory_ = nullptr;

// static
DeviceReenroller::Factory* DeviceReenroller::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void DeviceReenroller::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

DeviceReenroller::Factory::~Factory() = default;

std::unique_ptr<DeviceReenroller> DeviceReenroller::Factory::BuildInstance(
    device_sync::DeviceSyncClient* device_sync_client,
    const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
    std::unique_ptr<base::OneShotTimer> timer) {
  return base::WrapUnique(new DeviceReenroller(
      device_sync_client, gcm_device_info_provider, std::move(timer)));
}

DeviceReenroller::~DeviceReenroller() = default;

DeviceReenroller::DeviceReenroller(
    device_sync::DeviceSyncClient* device_sync_client,
    const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
    std::unique_ptr<base::OneShotTimer> timer)
    : device_sync_client_(device_sync_client),
      gcm_supported_software_features_(
          ComputeSupportedSoftwareFeaturesSortedDedupedListFromGcmDeviceInfo(
              gcm_device_info_provider->GetGcmDeviceInfo())),
      timer_(std::move(timer)) {
  // If the current set of supported software features from GcmDeviceInfo
  // differs from that of the local device metadata on the CryptAuth server,
  // attempt re-enrollment. Note: Both lists here are sorted and duplicate-free.
  if (gcm_supported_software_features_ !=
      ComputeSupportedSoftwareFeaturesSortedDedupedListFromLocalDeviceMetadata(
          *device_sync_client_->GetLocalDeviceMetadata())) {
    AttemptReenrollment();
  }
}

void DeviceReenroller::AttemptReenrollment() {
  DCHECK(!timer_->IsRunning());
  device_sync_client_->ForceEnrollmentNow(base::BindOnce(
      &DeviceReenroller::OnForceEnrollmentNowComplete, base::Unretained(this)));
}

void DeviceReenroller::AttemptDeviceSync() {
  DCHECK(!timer_->IsRunning());
  device_sync_client_->ForceSyncNow(base::BindOnce(
      &DeviceReenroller::OnForceSyncNowComplete, base::Unretained(this)));
}

void DeviceReenroller::OnForceEnrollmentNowComplete(bool success) {
  if (success) {
    PA_LOG(INFO) << "DeviceReenroller::OnForceEnrollmentNowComplete(): "
                 << "Forced enrollment attempt was successful. "
                 << "Syncing devices now.";
    AttemptDeviceSync();
    return;
  }
  PA_LOG(WARNING) << "DeviceReenroller::OnForceEnrollmentNowComplete(): "
                  << "Forced enrollment attempt was unsuccessful. Retrying in "
                  << kNumMinutesBetweenRetries << " minutes.";
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromMinutes(kNumMinutesBetweenRetries),
                base::BindOnce(&DeviceReenroller::AttemptReenrollment,
                               base::Unretained(this)));
}

void DeviceReenroller::OnForceSyncNowComplete(bool success) {
  // This is used to track if the device sync properly updated the local device
  // metadata to reflect the supported software features from GcmDeviceInfo.
  bool local_device_metadata_agrees =
      device_sync_client_->GetLocalDeviceMetadata() &&
      ComputeSupportedSoftwareFeaturesSortedDedupedListFromLocalDeviceMetadata(
          *device_sync_client_->GetLocalDeviceMetadata()) ==
          gcm_supported_software_features_;

  if (success && local_device_metadata_agrees) {
    PA_LOG(INFO) << "DeviceReenroller::OnForceSyncNowComplete(): "
                 << "Forced device sync attempt was successful.";
    return;
  }
  if (!success) {
    PA_LOG(WARNING) << "DeviceReenroller::OnForceSyncNowComplete(): "
                    << "Forced device sync attempt was unsuccessful. "
                    << "Retrying in " << kNumMinutesBetweenRetries
                    << " minutes.";
  } else {
    DCHECK(!local_device_metadata_agrees);
    PA_LOG(WARNING) << "DeviceReenroller::OnForceSyncNowComplete(): "
                    << "The local device metadata's supported software "
                    << "features do not agree with the set extracted from GCM "
                    << "device info. Retrying in " << kNumMinutesBetweenRetries
                    << " minutes.";
  }
  timer_->Start(FROM_HERE,
                base::TimeDelta::FromMinutes(kNumMinutesBetweenRetries),
                base::BindOnce(&DeviceReenroller::AttemptDeviceSync,
                               base::Unretained(this)));
}

}  // namespace multidevice_setup

}  // namespace chromeos
