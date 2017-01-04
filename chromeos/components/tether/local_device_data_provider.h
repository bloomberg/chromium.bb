// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_LOCAL_DEVICE_DATA_PROVIDER_H
#define CHROMEOS_COMPONENTS_TETHER_LOCAL_DEVICE_DATA_PROVIDER_H

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"

namespace cryptauth {
class BeaconSeed;
class ExternalDeviceInfo;
class CryptAuthDeviceManager;
class CryptAuthEnrollmentManager;
}

namespace chromeos {

namespace tether {

// Fetches CryptAuth data about the local device (i.e., the device on which this
// code is running) for the current user (i.e., the one which is logged-in).
class LocalDeviceDataProvider {
 public:
  LocalDeviceDataProvider(
      const cryptauth::CryptAuthDeviceManager* cryptauth_device_manager,
      const cryptauth::CryptAuthEnrollmentManager*
          cryptauth_enrollment_manager);
  virtual ~LocalDeviceDataProvider();

  // Fetches the public key and/or the beacon seeds for the local device.
  // Returns whether the operation succeeded. If |nullptr| is passed as a
  // parameter, the associated data will not be fetched.
  virtual bool GetLocalDeviceData(
      std::string* public_key_out,
      std::vector<cryptauth::BeaconSeed>* beacon_seeds_out) const;

 private:
  friend class LocalDeviceDataProviderTest;

  class LocalDeviceDataProviderDelegate {
   public:
    virtual ~LocalDeviceDataProviderDelegate() {}
    virtual std::string GetUserPublicKey() const = 0;
    virtual std::vector<cryptauth::ExternalDeviceInfo> GetSyncedDevices()
        const = 0;
  };

  class LocalDeviceDataProviderDelegateImpl
      : public LocalDeviceDataProviderDelegate {
   public:
    LocalDeviceDataProviderDelegateImpl(
        const cryptauth::CryptAuthDeviceManager* cryptauth_device_manager,
        const cryptauth::CryptAuthEnrollmentManager*
            cryptauth_enrollment_manager);
    ~LocalDeviceDataProviderDelegateImpl() override;

    std::string GetUserPublicKey() const override;
    std::vector<cryptauth::ExternalDeviceInfo> GetSyncedDevices()
        const override;

   private:
    // Not owned and must outlive this instance.
    const cryptauth::CryptAuthDeviceManager* const cryptauth_device_manager_;

    // Not owned and must outlive this instance.
    const cryptauth::CryptAuthEnrollmentManager* const
        cryptauth_enrollment_manager_;
  };

  LocalDeviceDataProvider(
      std::unique_ptr<LocalDeviceDataProviderDelegate> delegate);

  std::unique_ptr<LocalDeviceDataProviderDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(LocalDeviceDataProvider);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_LOCAL_DEVICE_DATA_PROVIDER_H
