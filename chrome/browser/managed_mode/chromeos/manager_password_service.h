// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_CHROMEOS_MANAGER_PASSWORD_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_CHROMEOS_MANAGER_PASSWORD_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/auth/extended_authenticator.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

// Handles managed user password change that is detected while manager is
// signed in.
// It uses manager's master key to authorize update of managed user's key.
// Edge case: Pre-M35 supervised users don't have correct labels for keys.
// After new managed user key is added, migration is done in following way:
// 1) Master key is added with correct label
// 2) Old managed user's key is deleted.
// 3) Old master key is deleted.
class ManagerPasswordService
    : public KeyedService,
      public chromeos::ExtendedAuthenticator::AuthStatusConsumer {
 public:
  ManagerPasswordService();
  virtual ~ManagerPasswordService();

  virtual void Shutdown() OVERRIDE;

  void Init(const std::string& user_id,
            ManagedUserSyncService* user_service,
            ManagedUserSharedSettingsService* service);

  // chromeos::ExtendedAuthenticator::AuthStatusConsumer overrides:
  virtual void OnAuthenticationFailure(ExtendedAuthenticator::AuthState state)
      OVERRIDE;

 private:
  void OnSharedSettingsChange(const std::string& mu_id, const std::string& key);
  void GetManagedUsersCallback(const std::string& sync_mu_id,
                               const std::string& user_id,
                               scoped_ptr<base::DictionaryValue> password_data,
                               const base::DictionaryValue* managed_users);
  void OnAddKeySuccess(const UserContext& master_key_context,
                       const std::string& user_id,
                       scoped_ptr<base::DictionaryValue> password_data);
  void OnContextTransformed(const UserContext& master_key_context);
  void OnNewManagerKeySuccess(const UserContext& master_key_context);
  void OnOldManagedUserKeyDeleted(const UserContext& master_key_context);
  void OnOldManagerKeyDeleted(const UserContext& master_key_context);

  // Cached value from Init().
  // User id of currently logged in user, that have managed users on device.
  std::string user_id_;
  ManagedUserSyncService* user_service_;
  ManagedUserSharedSettingsService* settings_service_;

  scoped_ptr<ManagedUserSharedSettingsService::ChangeCallbackList::Subscription>
      settings_service_subscription_;

  scoped_refptr<ExtendedAuthenticator> authenticator_;

  base::WeakPtrFactory<ManagerPasswordService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagerPasswordService);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_MANAGED_MODE_CHROMEOS_MANAGER_PASSWORD_SERVICE_H_
