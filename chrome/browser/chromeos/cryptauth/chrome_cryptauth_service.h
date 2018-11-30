// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CRYPTAUTH_CHROME_CRYPTAUTH_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CRYPTAUTH_CHROME_CRYPTAUTH_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/cryptauth_service.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "services/identity/public/cpp/identity_manager.h"

class Profile;

namespace cryptauth {
class CryptAuthGCMManager;
}  // namespace cryptauth

namespace chromeos {

// Implementation of cryptauth::CryptAuthService.
class ChromeCryptAuthService
    : public KeyedService,
      public cryptauth::CryptAuthService,
      public cryptauth::CryptAuthEnrollmentManager::Observer,
      public identity::IdentityManager::Observer {
 public:
  static std::unique_ptr<ChromeCryptAuthService> Create(Profile* profile);
  ~ChromeCryptAuthService() override;

  // KeyedService:
  void Shutdown() override;

  // cryptauth::CryptAuthService:
  cryptauth::CryptAuthDeviceManager* GetCryptAuthDeviceManager() override;
  cryptauth::CryptAuthEnrollmentManager* GetCryptAuthEnrollmentManager()
      override;
  cryptauth::DeviceClassifier GetDeviceClassifier() override;
  std::string GetAccountId() override;
  std::unique_ptr<cryptauth::CryptAuthClientFactory>
  CreateCryptAuthClientFactory() override;

  // cryptauth::CryptAuthEnrollmentManager::Observer:
  void OnEnrollmentFinished(bool success) override;

 protected:
  // Note: ChromeCryptAuthServiceFactory DependsOn(OAuth2TokenServiceFactory),
  // so |token_service| is guaranteed to outlast this service.
  ChromeCryptAuthService(
      std::unique_ptr<cryptauth::CryptAuthClientFactory> client_factory,
      std::unique_ptr<cryptauth::CryptAuthGCMManager> gcm_manager,
      std::unique_ptr<cryptauth::CryptAuthDeviceManager> device_manager,
      std::unique_ptr<cryptauth::CryptAuthEnrollmentManager> enrollment_manager,
      Profile* profile,
      identity::IdentityManager* identity_manager);

 private:
  // identity::IdentityManager::Observer:
  void OnPrimaryAccountSet(const AccountInfo& primary_account_info) override;
  void OnRefreshTokenUpdatedForAccount(
      const AccountInfo& account_info) override;

  void OnAuthenticationStateChanged();
  void PerformEnrollmentAndDeviceSyncIfPossible();
  void OnPrefsChanged();

  std::unique_ptr<cryptauth::CryptAuthClientFactory> client_factory_;
  std::unique_ptr<cryptauth::CryptAuthGCMManager> gcm_manager_;
  std::unique_ptr<cryptauth::CryptAuthEnrollmentManager> enrollment_manager_;
  std::unique_ptr<cryptauth::CryptAuthDeviceManager> device_manager_;
  Profile* profile_;
  identity::IdentityManager* identity_manager_;
  PrefChangeRegistrar registrar_;

  base::WeakPtrFactory<ChromeCryptAuthService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCryptAuthService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CRYPTAUTH_CHROME_CRYPTAUTH_SERVICE_H_
