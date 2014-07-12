// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/owner_key_util.h"
#include "chromeos/tpm_token_loader.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace chromeos {

// This class reloads owner key from profile NSS slots.
//
// TODO (ygorshenin@): move write path for device settings here
// (crbug.com/230018).
class OwnerSettingsService : public DeviceSettingsService::PrivateKeyDelegate,
                             public KeyedService,
                             public content::NotificationObserver,
                             public TPMTokenLoader::Observer {
 public:
  virtual ~OwnerSettingsService();

  base::WeakPtr<OwnerSettingsService> as_weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  // DeviceSettingsService::PrivateKeyDelegate implementation:
  virtual bool IsOwner() OVERRIDE;
  virtual void IsOwnerAsync(const IsOwnerCallback& callback) OVERRIDE;
  virtual bool AssembleAndSignPolicyAsync(
      scoped_ptr<enterprise_management::PolicyData> policy,
      const AssembleAndSignPolicyCallback& callback) OVERRIDE;

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // TPMTokenLoader::Observer:
  virtual void OnTPMTokenReady() OVERRIDE;

  // Checks if the user is the device owner, without the user profile having to
  // been initialized. Should be used only if login state is in safe mode.
  static void IsOwnerForSafeModeAsync(const std::string& user_id,
                                      const std::string& user_hash,
                                      const IsOwnerCallback& callback);

  static void SetOwnerKeyUtilForTesting(
      const scoped_refptr<OwnerKeyUtil>& owner_key_util);

  static void SetDeviceSettingsServiceForTesting(
      DeviceSettingsService* device_settings_service);

 private:
  friend class OwnerSettingsServiceFactory;

  explicit OwnerSettingsService(Profile* profile);

  // Reloads private key from profile's NSS slots. Responds via call
  // to OnPrivateKeyLoaded().
  void ReloadPrivateKey();

  // Called when ReloadPrivateKey() completes it's work.
  void OnPrivateKeyLoaded(scoped_ptr<crypto::RSAPrivateKey> private_key);

  // Returns testing instance of OwnerKeyUtil when it's set, otherwise
  // returns |owner_key_util_|.
  scoped_refptr<OwnerKeyUtil> GetOwnerKeyUtil();

  // Returns testing instance of DeviceSettingsService when it's set,
  // otherwise returns pointer to a singleton instance, when it's
  // initialized.
  DeviceSettingsService* GetDeviceSettingsService();

  // Profile this service instance belongs to.
  Profile* profile_;

  scoped_refptr<PrivateKey> private_key_;

  scoped_refptr<OwnerKeyUtil> owner_key_util_;

  std::vector<IsOwnerCallback> pending_is_owner_callbacks_;

  // Whether profile still needs to be initialized.
  bool waiting_for_profile_creation_;

  // Whether TPM token still needs to be initialized.
  bool waiting_for_tpm_token_;

  content::NotificationRegistrar registrar_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<OwnerSettingsService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_H_
