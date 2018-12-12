// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_FAKE_CRYPTAUTH_SERVICE_H_
#define COMPONENTS_CRYPTAUTH_FAKE_CRYPTAUTH_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "components/cryptauth/cryptauth_service.h"

namespace chromeos {
namespace device_sync {
class CryptAuthClientFactory;
class CryptAuthDeviceManager;
class CryptAuthEnrollmentManager;
}  // namespace device_sync
}  // namespace chromeos

namespace cryptauth {

// Service which provides access to various CryptAuth singletons.
class FakeCryptAuthService : public CryptAuthService {
 public:
  FakeCryptAuthService();
  ~FakeCryptAuthService() override;

  void set_cryptauth_device_manager(
      chromeos::device_sync::CryptAuthDeviceManager* cryptauth_device_manager) {
    cryptauth_device_manager_ = cryptauth_device_manager;
  }

  void set_cryptauth_enrollment_manager(
      chromeos::device_sync::CryptAuthEnrollmentManager*
          cryptauth_enrollment_manager) {
    cryptauth_enrollment_manager_ = cryptauth_enrollment_manager;
  }

  void set_device_classifier(const DeviceClassifier& device_classifier) {
    device_classifier_ = device_classifier;
  }

  void set_account_id(const std::string& account_id) {
    account_id_ = account_id;
  }

  // CryptAuthService:
  chromeos::device_sync::CryptAuthDeviceManager* GetCryptAuthDeviceManager()
      override;
  chromeos::device_sync::CryptAuthEnrollmentManager*
  GetCryptAuthEnrollmentManager() override;
  DeviceClassifier GetDeviceClassifier() override;
  std::string GetAccountId() override;
  std::unique_ptr<chromeos::device_sync::CryptAuthClientFactory>
  CreateCryptAuthClientFactory() override;

 private:
  chromeos::device_sync::CryptAuthDeviceManager* cryptauth_device_manager_;
  chromeos::device_sync::CryptAuthEnrollmentManager*
      cryptauth_enrollment_manager_;
  DeviceClassifier device_classifier_;
  std::string account_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeCryptAuthService);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_FAKE_CRYPTAUTH_SERVICE_H_
