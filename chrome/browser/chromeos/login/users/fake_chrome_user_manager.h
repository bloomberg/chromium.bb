// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_CHROME_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_CHROME_USER_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/user_manager_interface.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager_base.h"

namespace chromeos {

class FakeSupervisedUserManager;

// Fake chrome user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class FakeChromeUserManager : public user_manager::FakeUserManager,
                              public UserManagerInterface {
 public:
  FakeChromeUserManager();
  ~FakeChromeUserManager() override;

  // Create and add a kiosk app user.
  void AddKioskAppUser(const std::string& kiosk_app_username);

  // Create and add a public account user.
  const user_manager::User* AddPublicAccountUser(const std::string& email);

  // Calculates the user name hash and calls UserLoggedIn to login a user.
  void LoginUser(const std::string& email);

  // UserManager overrides.
  user_manager::UserList GetUsersAllowedForMultiProfile() const override;

  // user_manager::FakeUserManager override.
  const user_manager::User* AddUser(const std::string& email) override;

  // UserManagerInterface implementation.
  BootstrapManager* GetBootstrapManager() override;
  MultiProfileUserController* GetMultiProfileUserController() override;
  UserImageManager* GetUserImageManager(const std::string& user_id) override;
  SupervisedUserManager* GetSupervisedUserManager() override;
  void SetUserFlow(const std::string& email, UserFlow* flow) override;
  UserFlow* GetCurrentUserFlow() const override;
  UserFlow* GetUserFlow(const std::string& email) const override;
  void ResetUserFlow(const std::string& email) override;
  user_manager::UserList GetUsersAllowedForSupervisedUsersCreation()
      const override;
  void SwitchActiveUser(const std::string& email) override;
  const std::string& GetOwnerEmail() const override;
  void SessionStarted() override;
  void RemoveUser(const std::string& email,
                  user_manager::RemoveUserDelegate* delegate) override;

  void set_owner_email(const std::string& owner_email) {
    owner_email_ = owner_email;
  }

  void set_bootstrap_manager(BootstrapManager* bootstrap_manager) {
    bootstrap_manager_ = bootstrap_manager;
  }

  void set_multi_profile_user_controller(
      MultiProfileUserController* controller) {
    multi_profile_user_controller_ = controller;
  }

 private:
  // Lazily creates default user flow.
  UserFlow* GetDefaultUserFlow() const;

  scoped_ptr<FakeSupervisedUserManager> supervised_user_manager_;
  std::string owner_email_;

  BootstrapManager* bootstrap_manager_;
  MultiProfileUserController* multi_profile_user_controller_;

  typedef std::map<std::string, UserFlow*> FlowMap;

  // Lazy-initialized default flow.
  mutable scoped_ptr<UserFlow> default_flow_;

  // Specific flows by user e-mail.
  // Keys should be canonicalized before access.
  FlowMap specific_flows_;

  DISALLOW_COPY_AND_ASSIGN(FakeChromeUserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_CHROME_USER_MANAGER_H_
