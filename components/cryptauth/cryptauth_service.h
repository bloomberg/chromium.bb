// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_CRYPTAUTH_SERVICE_H_
#define COMPONENTS_CRYPTAUTH_CRYPTAUTH_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace device_sync {
class CryptAuthClientFactory;
class CryptAuthDeviceManager;
class CryptAuthEnrollmentManager;
}  // namespace device_sync
}  // namespace chromeos

namespace cryptauth {

// Service which provides access to various CryptAuth singletons.
class CryptAuthService {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  virtual chromeos::device_sync::CryptAuthDeviceManager*
  GetCryptAuthDeviceManager() = 0;
  virtual chromeos::device_sync::CryptAuthEnrollmentManager*
  GetCryptAuthEnrollmentManager() = 0;
  virtual DeviceClassifier GetDeviceClassifier() = 0;
  virtual std::string GetAccountId() = 0;
  virtual std::unique_ptr<chromeos::device_sync::CryptAuthClientFactory>
  CreateCryptAuthClientFactory() = 0;

 protected:
  CryptAuthService() = default;
  virtual ~CryptAuthService() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CryptAuthService);
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_CRYPTAUTH_SERVICE_H_
