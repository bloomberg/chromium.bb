// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_CHROMEOS_MANAGER_PASSWORD_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_CHROMEOS_MANAGER_PASSWORD_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "components/keyed_service/core/keyed_service.h"

class ManagerPasswordService : public KeyedService {
 public:
  ManagerPasswordService();
  virtual ~ManagerPasswordService();

  virtual void Shutdown() OVERRIDE;

  void Init(const std::string& user_id,
            ManagedUserSyncService* user_service,
            ManagedUserSharedSettingsService* service);
 private:
  void OnSharedSettingsChange(const std::string& mu_id, const std::string& key);
  void GetManagedUsersCallback(const std::string& sync_mu_id,
                               const std::string& user_id,
                               const base::DictionaryValue* password_data,
                               const base::DictionaryValue* managed_users);

  // Cached value from Init().
  // User id of currently logged in user, that have managed users on device.
  std::string user_id_;
  ManagedUserSyncService* user_service_;
  ManagedUserSharedSettingsService* settings_service_;

  scoped_ptr<ManagedUserSharedSettingsService::ChangeCallbackList::Subscription>
      settings_service_subscription_;

  base::WeakPtrFactory<ManagerPasswordService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagerPasswordService);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_CHROMEOS_MANAGER_PASSWORD_SERVICE_H_
