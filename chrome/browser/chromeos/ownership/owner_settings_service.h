// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_H_

#include <deque>
#include <vector>

#include "base/callback.h"
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

class SessionManagerOperation;

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
  virtual void SignAndStoreAsync(
      scoped_ptr<enterprise_management::ChromeDeviceSettingsProto> settings,
      const base::Closure& callback) OVERRIDE;
  virtual void SetManagementSettingsAsync(
      enterprise_management::PolicyData::ManagementMode management_mode,
      const std::string& request_token,
      const std::string& device_id,
      const base::Closure& callback) OVERRIDE;

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
  void OnPrivateKeyLoaded(scoped_refptr<PublicKey> public_key,
                          scoped_refptr<PrivateKey> private_key);

  // Puts request to perform sign-and-store operation in the queue.
  void EnqueueSignAndStore(scoped_ptr<enterprise_management::PolicyData> policy,
                           const base::Closure& callback);

  // Performs next operation in the queue.
  void StartNextOperation();

  // Called when sign-and-store operation completes it's work.
  void HandleCompletedOperation(const base::Closure& callback,
                                SessionManagerOperation* operation,
                                DeviceSettingsService::Status status);

  // Called when it's not possible to store settings.
  void HandleError(DeviceSettingsService::Status status,
                   const base::Closure& callback);

  // Returns testing instance of OwnerKeyUtil when it's set, otherwise
  // returns |owner_key_util_|.
  scoped_refptr<OwnerKeyUtil> GetOwnerKeyUtil();

  // Returns testing instance of DeviceSettingsService when it's set,
  // otherwise returns pointer to a singleton instance, when it's
  // initialized.
  DeviceSettingsService* GetDeviceSettingsService();

  // Profile this service instance belongs to.
  Profile* profile_;

  // User ID this service instance belongs to.
  std::string user_id_;

  scoped_refptr<PublicKey> public_key_;

  scoped_refptr<PrivateKey> private_key_;

  scoped_refptr<OwnerKeyUtil> owner_key_util_;

  std::vector<IsOwnerCallback> pending_is_owner_callbacks_;

  // Whether profile still needs to be initialized.
  bool waiting_for_profile_creation_;

  // Whether TPM token still needs to be initialized.
  bool waiting_for_tpm_token_;

  // The queue of pending sign-and-store operations. The first operation on the
  // queue is currently active; it gets removed and destroyed once it completes.
  std::deque<SessionManagerOperation*> pending_operations_;

  content::NotificationRegistrar registrar_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<OwnerSettingsService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_H_
