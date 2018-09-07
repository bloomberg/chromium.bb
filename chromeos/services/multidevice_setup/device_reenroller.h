// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_DEVICE_REENROLLER_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_DEVICE_REENROLLER_H_

#include <memory>

#include "base/macros.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace cryptauth {
class GcmDeviceInfoProvider;
}  // namespace cryptauth

namespace chromeos {

namespace device_sync {
class DeviceSyncClient;
}  // namespace device_sync

namespace multidevice_setup {

// The DeviceReenroller constructor re-enrolls and syncs the device if the set
// of supported SoftwareFeatures in the current GCM device info differs from
// that of the local device metadata on the CryptAuth server.
class DeviceReenroller {
 public:
  class Factory {
   public:
    static Factory* Get();
    static void SetFactoryForTesting(Factory* test_factory);
    virtual ~Factory();
    virtual std::unique_ptr<DeviceReenroller> BuildInstance(
        device_sync::DeviceSyncClient* device_sync_client,
        const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
        std::unique_ptr<base::OneShotTimer> timer =
            std::make_unique<base::OneShotTimer>());

   private:
    static Factory* test_factory_;
  };

  virtual ~DeviceReenroller();

 private:
  DeviceReenroller(
      device_sync::DeviceSyncClient* device_sync_client,
      const cryptauth::GcmDeviceInfoProvider* gcm_device_info_provider,
      std::unique_ptr<base::OneShotTimer> timer);

  void AttemptReenrollment();
  void AttemptDeviceSync();

  // If the re-enrollment was successful, force a device sync; otherwise, retry
  // re-enrollment every 5 minutes or until success.
  void OnForceEnrollmentNowComplete(bool success);
  // If the device sync was successful and the list of supported software
  // features on the CryptAuth server now agrees with the list of supported
  // software features in GcmDeviceInfo, log the success; otherwise, retry
  // device sync every 5 minutes or until success.
  void OnForceSyncNowComplete(bool success);

  device_sync::DeviceSyncClient* device_sync_client_;
  // The sorted and deduped list of supported software features extracted from
  // GcmDeviceInfo.
  std::vector<cryptauth::SoftwareFeature> gcm_supported_software_features_;
  std::unique_ptr<base::OneShotTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceReenroller);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_DEVICE_REENROLLER_H_
