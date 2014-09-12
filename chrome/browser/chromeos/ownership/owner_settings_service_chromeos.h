// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_H_

#include <deque>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ownership/owner_key_util.h"
#include "components/ownership/owner_settings_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace ownership {
class OwnerKeyUtil;
}

namespace chromeos {

class SessionManagerOperation;

// The class is a profile-keyed service which holds public/private
// keypair corresponds to a profile. The keypair is reloaded automatically when
// profile is created and TPM token is ready. Note that the private part of a
// key can be loaded only for the owner.
//
// TODO (ygorshenin@): move write path for device settings here
// (crbug.com/230018).
class OwnerSettingsServiceChromeOS : public ownership::OwnerSettingsService,
                                     public content::NotificationObserver,
                                     public SessionManagerClient::Observer {
 public:
  virtual ~OwnerSettingsServiceChromeOS();

  void OnTPMTokenReady(bool tpm_token_enabled);

  // ownership::OwnerSettingsService implementation:
  virtual void SignAndStorePolicyAsync(
      scoped_ptr<enterprise_management::PolicyData> policy,
      const base::Closure& callback) OVERRIDE;

  // NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // SessionManagerClient::Observer:
  virtual void OwnerKeySet(bool success) OVERRIDE;

  // Checks if the user is the device owner, without the user profile having to
  // been initialized. Should be used only if login state is in safe mode.
  static void IsOwnerForSafeModeAsync(
      const std::string& user_hash,
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util,
      const IsOwnerCallback& callback);

  static void SetDeviceSettingsServiceForTesting(
      DeviceSettingsService* device_settings_service);

 private:
  friend class OwnerSettingsServiceChromeOSFactory;

  OwnerSettingsServiceChromeOS(
      Profile* profile,
      const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util);

  // OwnerSettingsService protected interface overrides:

  // Reloads private key from profile's NSS slots, responds via |callback|.
  virtual void ReloadKeypairImpl(const base::Callback<
      void(const scoped_refptr<ownership::PublicKey>& public_key,
           const scoped_refptr<ownership::PrivateKey>& private_key)>& callback)
      OVERRIDE;

  // Possibly notifies DeviceSettingsService that owner's keypair is loaded.
  virtual void OnPostKeypairLoadedActions() OVERRIDE;

  // Performs next operation in the queue.
  void StartNextOperation();

  // Called when sign-and-store operation completes it's work.
  void HandleCompletedOperation(const base::Closure& callback,
                                SessionManagerOperation* operation,
                                DeviceSettingsService::Status status);

  // Profile this service instance belongs to.
  Profile* profile_;

  // User ID this service instance belongs to.
  std::string user_id_;

  // Whether profile still needs to be initialized.
  bool waiting_for_profile_creation_;

  // Whether TPM token still needs to be initialized.
  bool waiting_for_tpm_token_;

  // The queue of pending sign-and-store operations. The first operation on the
  // queue is currently active; it gets removed and destroyed once it completes.
  std::deque<SessionManagerOperation*> pending_operations_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<OwnerSettingsServiceChromeOS> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(OwnerSettingsServiceChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OWNERSHIP_OWNER_SETTINGS_SERVICE_CHROMEOS_H_
