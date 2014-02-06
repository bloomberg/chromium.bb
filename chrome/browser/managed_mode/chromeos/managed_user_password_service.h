// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_CHROMEOS_MANAGED_USER_PASSWORD_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_CHROMEOS_MANAGED_USER_PASSWORD_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class ManagedUserPasswordService : public BrowserContextKeyedService {
 public:
  ManagedUserPasswordService();
  virtual ~ManagedUserPasswordService();

  virtual void Shutdown() OVERRIDE;

  void Init(const std::string& user_id,
            ManagedUserSharedSettingsService* service);
 private:
  void OnSharedSettingsChange(const std::string& mu_id, const std::string& key);

  // Cached value from Init().
  // User id of currently logged in managed user.
  std::string user_id_;
  ManagedUserSharedSettingsService* settings_service_;

  scoped_ptr<ManagedUserSharedSettingsService::ChangeCallbackList::Subscription>
      settings_service_subscription_;

  base::WeakPtrFactory<ManagedUserPasswordService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserPasswordService);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_CHROMEOS_MANAGED_USER_PASSWORD_SERVICE_H_
