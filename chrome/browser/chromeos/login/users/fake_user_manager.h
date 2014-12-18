// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_USER_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"

namespace chromeos {

class FakeSupervisedUserManager;

// Fake user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class FakeUserManager : public ChromeUserManager {
 public:
  FakeUserManager();
  virtual ~FakeUserManager();

  // Create and add a new user.
  const user_manager::User* AddUser(const std::string& email);

  // Create and add a kiosk app user.
  void AddKioskAppUser(const std::string& kiosk_app_username);

  // Create and add a public account user.
  const user_manager::User* AddPublicAccountUser(const std::string& email);

  // Calculates the user name hash and calls UserLoggedIn to login a user.
  void LoginUser(const std::string& email);

  // ChromeUserManager overrides.
  virtual MultiProfileUserController* GetMultiProfileUserController() override;
  virtual UserImageManager* GetUserImageManager(
      const std::string& user_id) override;
  virtual SupervisedUserManager* GetSupervisedUserManager() override;
  virtual void SetUserFlow(const std::string& email, UserFlow* flow) override {}
  virtual UserFlow* GetCurrentUserFlow() const override;
  virtual UserFlow* GetUserFlow(const std::string& email) const override;
  virtual void ResetUserFlow(const std::string& email) override {}

  // UserManager overrides.
  virtual const user_manager::UserList& GetUsers() const override;
  virtual user_manager::UserList GetUsersAllowedForMultiProfile()
      const override;
  virtual user_manager::UserList GetUsersAllowedForSupervisedUsersCreation()
      const override;
  virtual const user_manager::UserList& GetLoggedInUsers() const override;

  // Set the user as logged in.
  virtual void UserLoggedIn(const std::string& email,
                            const std::string& username_hash,
                            bool browser_restart) override;

  virtual const user_manager::User* GetActiveUser() const override;
  virtual user_manager::User* GetActiveUser() override;
  virtual void SwitchActiveUser(const std::string& email) override;
  virtual void SaveUserDisplayName(const std::string& username,
      const base::string16& display_name) override;

  // Not implemented.
  virtual void UpdateUserAccountData(
      const std::string& user_id,
      const UserAccountData& account_data) override {}
  virtual void Shutdown() override {}
  virtual const user_manager::UserList& GetLRULoggedInUsers() const override;
  virtual user_manager::UserList GetUnlockUsers() const override;
  virtual const std::string& GetOwnerEmail() const override;
  virtual void SessionStarted() override {}
  virtual void RemoveUser(const std::string& email,
                          user_manager::RemoveUserDelegate* delegate) override {
  }
  virtual void RemoveUserFromList(const std::string& email) override;
  virtual bool IsKnownUser(const std::string& email) const override;
  virtual const user_manager::User* FindUser(
      const std::string& email) const override;
  virtual user_manager::User* FindUserAndModify(
      const std::string& email) override;
  virtual const user_manager::User* GetLoggedInUser() const override;
  virtual user_manager::User* GetLoggedInUser() override;
  virtual const user_manager::User* GetPrimaryUser() const override;
  virtual void SaveUserOAuthStatus(
      const std::string& username,
      user_manager::User::OAuthTokenStatus oauth_token_status) override {}
  virtual void SaveForceOnlineSignin(const std::string& user_id,
                                     bool force_online_signin) override {}
  virtual base::string16 GetUserDisplayName(
      const std::string& username) const override;
  virtual void SaveUserDisplayEmail(const std::string& username,
      const std::string& display_email) override {}
  virtual std::string GetUserDisplayEmail(
      const std::string& username) const override;
  virtual bool IsCurrentUserOwner() const override;
  virtual bool IsCurrentUserNew() const override;
  virtual bool IsCurrentUserNonCryptohomeDataEphemeral() const override;
  virtual bool CanCurrentUserLock() const override;
  virtual bool IsUserLoggedIn() const override;
  virtual bool IsLoggedInAsUserWithGaiaAccount() const override;
  virtual bool IsLoggedInAsPublicAccount() const override;
  virtual bool IsLoggedInAsGuest() const override;
  virtual bool IsLoggedInAsSupervisedUser() const override;
  virtual bool IsLoggedInAsKioskApp() const override;
  virtual bool IsLoggedInAsStub() const override;
  virtual bool IsSessionStarted() const override;
  virtual bool IsUserNonCryptohomeDataEphemeral(
      const std::string& email) const override;
  virtual void AddObserver(Observer* obs) override {}
  virtual void RemoveObserver(Observer* obs) override {}
  virtual void AddSessionStateObserver(
      UserSessionStateObserver* obs) override {}
  virtual void RemoveSessionStateObserver(
      UserSessionStateObserver* obs) override {}
  virtual void NotifyLocalStateChanged() override {}
  virtual bool AreSupervisedUsersAllowed() const override;

  // UserManagerBase overrides:
  virtual bool AreEphemeralUsersEnabled() const override;
  virtual const std::string& GetApplicationLocale() const override;
  virtual PrefService* GetLocalState() const override;
  virtual void HandleUserOAuthTokenStatusChange(
      const std::string& user_id,
      user_manager::User::OAuthTokenStatus status) const override {}
  virtual bool IsEnterpriseManaged() const override;
  virtual void LoadPublicAccounts(
      std::set<std::string>* public_sessions_set) override {}
  virtual void PerformPreUserListLoadingActions() override {}
  virtual void PerformPostUserListLoadingActions() override {}
  virtual void PerformPostUserLoggedInActions(bool browser_restart) override {}
  virtual bool IsDemoApp(const std::string& user_id) const override;
  virtual bool IsKioskApp(const std::string& user_id) const override;
  virtual bool IsPublicAccountMarkedForRemoval(
      const std::string& user_id) const override;
  virtual void DemoAccountLoggedIn() override {}
  virtual void KioskAppLoggedIn(const std::string& app_id) override {}
  virtual void PublicAccountUserLoggedIn(user_manager::User* user) override {}
  virtual void SupervisedUserLoggedIn(const std::string& user_id) override {}

  void set_owner_email(const std::string& owner_email) {
    owner_email_ = owner_email;
  }

  void set_multi_profile_user_controller(
      MultiProfileUserController* controller) {
    multi_profile_user_controller_ = controller;
  }

 private:
  // We use this internal function for const-correctness.
  user_manager::User* GetActiveUserInternal() const;

  scoped_ptr<FakeSupervisedUserManager> supervised_user_manager_;
  user_manager::UserList user_list_;
  user_manager::UserList logged_in_users_;
  std::string owner_email_;
  user_manager::User* primary_user_;

  // If set this is the active user. If empty, the first created user is the
  // active user.
  std::string active_user_id_;
  MultiProfileUserController* multi_profile_user_controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_USER_MANAGER_H_
