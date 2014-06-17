// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_SUPERVISED_USER_PASSWORD_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_SUPERVISED_USER_PASSWORD_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {

class SupervisedUserPasswordService : public KeyedService {
 public:
  SupervisedUserPasswordService();
  virtual ~SupervisedUserPasswordService();

  virtual void Shutdown() OVERRIDE;

  void Init(const std::string& user_id,
            SupervisedUserSharedSettingsService* service);
 private:
  void OnSharedSettingsChange(const std::string& su_id, const std::string& key);

  // Cached value from Init().
  // User id of currently logged in supervised user.
  std::string user_id_;
  SupervisedUserSharedSettingsService* settings_service_;

  scoped_ptr<SupervisedUserSharedSettingsService::ChangeCallbackList::
                 Subscription>
      settings_service_subscription_;

  base::WeakPtrFactory<SupervisedUserPasswordService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserPasswordService);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_SUPERVISED_USER_PASSWORD_SERVICE_H_
