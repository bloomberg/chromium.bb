// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRYPTAUTH_CHROME_CRYPTAUTH_SERVICE_H_
#define CHROME_BROWSER_CRYPTAUTH_CHROME_CRYPTAUTH_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/cryptauth/cryptauth_service.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace cryptauth {
class CryptAuthGCMManager;
}  // namespace cryptauth

// Implementation of cryptauth::CryptAuthService.
class ChromeCryptAuthService : public KeyedService,
                               public cryptauth::CryptAuthService {
 public:
  static std::unique_ptr<ChromeCryptAuthService> Create(Profile* profile);
  ~ChromeCryptAuthService() override;

  // cryptauth::CryptAuthService:
  cryptauth::CryptAuthDeviceManager* GetCryptAuthDeviceManager() override;
  cryptauth::CryptAuthEnrollmentManager* GetCryptAuthEnrollmentManager()
      override;
  cryptauth::DeviceClassifier GetDeviceClassifier() override;

 protected:
  ChromeCryptAuthService(
      std::unique_ptr<cryptauth::CryptAuthGCMManager> gcm_manager,
      std::unique_ptr<cryptauth::CryptAuthDeviceManager> device_manager,
      std::unique_ptr<cryptauth::CryptAuthEnrollmentManager>
          enrollment_manager);

 private:
  std::unique_ptr<cryptauth::CryptAuthGCMManager> gcm_manager_;
  std::unique_ptr<cryptauth::CryptAuthEnrollmentManager> enrollment_manager_;
  std::unique_ptr<cryptauth::CryptAuthDeviceManager> device_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCryptAuthService);
};

#endif  // CHROME_BROWSER_CRYPTAUTH_CHROME_CRYPTAUTH_SERVICE_H_
