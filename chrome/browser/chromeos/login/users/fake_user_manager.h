// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_USER_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/user.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "components/user_manager/user_image/user_image.h"

namespace chromeos {

class FakeSupervisedUserManager;

// Fake user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class FakeUserManager : public UserManager {
 public:
  FakeUserManager();
  virtual ~FakeUserManager();

  // Create and add a new user.
  const User* AddUser(const std::string& email);

  // Create and add a kiosk app user.
  void AddKioskAppUser(const std::string& kiosk_app_username);

  // Create and add a public account user.
  const User* AddPublicAccountUser(const std::string& email);

  // Calculates the user name hash and calls UserLoggedIn to login a user.
  void LoginUser(const std::string& email);

  // UserManager overrides.
  virtual const UserList& GetUsers() const OVERRIDE;
  virtual UserList GetUsersAdmittedForMultiProfile() const OVERRIDE;
  virtual const UserList& GetLoggedInUsers() const OVERRIDE;

  // Set the user as logged in.
  virtual void UserLoggedIn(const std::string& email,
                            const std::string& username_hash,
                            bool browser_restart) OVERRIDE;

  virtual const User* GetActiveUser() const OVERRIDE;
  virtual User* GetActiveUser() OVERRIDE;
  virtual void SwitchActiveUser(const std::string& email) OVERRIDE;
  virtual void SaveUserDisplayName(const std::string& username,
      const base::string16& display_name) OVERRIDE;

  // Not implemented.
  virtual void UpdateUserAccountData(
      const std::string& user_id,
      const UserAccountData& account_data) OVERRIDE {}
  virtual void Shutdown() OVERRIDE {}
  virtual MultiProfileUserController* GetMultiProfileUserController() OVERRIDE;
  virtual UserImageManager* GetUserImageManager(
      const std::string& user_id) OVERRIDE;
  virtual SupervisedUserManager* GetSupervisedUserManager() OVERRIDE;
  virtual const UserList& GetLRULoggedInUsers() OVERRIDE;
  virtual UserList GetUnlockUsers() const OVERRIDE;
  virtual const std::string& GetOwnerEmail() OVERRIDE;
  virtual void SessionStarted() OVERRIDE {}
  virtual void RemoveUser(const std::string& email,
      RemoveUserDelegate* delegate) OVERRIDE {}
  virtual void RemoveUserFromList(const std::string& email) OVERRIDE;
  virtual bool IsKnownUser(const std::string& email) const OVERRIDE;
  virtual const User* FindUser(const std::string& email) const OVERRIDE;
  virtual User* FindUserAndModify(const std::string& email) OVERRIDE;
  virtual const User* GetLoggedInUser() const OVERRIDE;
  virtual User* GetLoggedInUser() OVERRIDE;
  virtual const User* GetPrimaryUser() const OVERRIDE;
  virtual void SaveUserOAuthStatus(
      const std::string& username,
      User::OAuthTokenStatus oauth_token_status) OVERRIDE {}
  virtual void SaveForceOnlineSignin(const std::string& user_id,
                                     bool force_online_signin) OVERRIDE {}
  virtual base::string16 GetUserDisplayName(
      const std::string& username) const OVERRIDE;
  virtual void SaveUserDisplayEmail(const std::string& username,
      const std::string& display_email) OVERRIDE {}
  virtual std::string GetUserDisplayEmail(
      const std::string& username) const OVERRIDE;
  virtual bool IsCurrentUserOwner() const OVERRIDE;
  virtual bool IsCurrentUserNew() const OVERRIDE;
  virtual bool IsCurrentUserNonCryptohomeDataEphemeral() const OVERRIDE;
  virtual bool CanCurrentUserLock() const OVERRIDE;
  virtual bool IsUserLoggedIn() const OVERRIDE;
  virtual bool IsLoggedInAsRegularUser() const OVERRIDE;
  virtual bool IsLoggedInAsDemoUser() const OVERRIDE;
  virtual bool IsLoggedInAsPublicAccount() const OVERRIDE;
  virtual bool IsLoggedInAsGuest() const OVERRIDE;
  virtual bool IsLoggedInAsSupervisedUser() const OVERRIDE;
  virtual bool IsLoggedInAsKioskApp() const OVERRIDE;
  virtual bool IsLoggedInAsStub() const OVERRIDE;
  virtual bool IsSessionStarted() const OVERRIDE;
  virtual bool IsUserNonCryptohomeDataEphemeral(
      const std::string& email) const OVERRIDE;
  virtual void SetUserFlow(const std::string& email, UserFlow* flow) OVERRIDE {}
  virtual UserFlow* GetCurrentUserFlow() const OVERRIDE;
  virtual UserFlow* GetUserFlow(const std::string& email) const OVERRIDE;
  virtual void ResetUserFlow(const std::string& email) OVERRIDE {}
  virtual void AddObserver(Observer* obs) OVERRIDE {}
  virtual void RemoveObserver(Observer* obs) OVERRIDE {}
  virtual void AddSessionStateObserver(
      UserSessionStateObserver* obs) OVERRIDE {}
  virtual void RemoveSessionStateObserver(
      UserSessionStateObserver* obs) OVERRIDE {}
  virtual void NotifyLocalStateChanged() OVERRIDE {}
  virtual bool AreSupervisedUsersAllowed() const OVERRIDE;

  void set_owner_email(const std::string& owner_email) {
    owner_email_ = owner_email;
  }

  void set_multi_profile_user_controller(
      MultiProfileUserController* controller) {
    multi_profile_user_controller_ = controller;
  }

 private:
  // We use this internal function for const-correctness.
  User* GetActiveUserInternal() const;

  scoped_ptr<FakeSupervisedUserManager> supervised_user_manager_;
  UserList user_list_;
  UserList logged_in_users_;
  std::string owner_email_;
  User* primary_user_;

  // If set this is the active user. If empty, the first created user is the
  // active user.
  std::string active_user_id_;
  MultiProfileUserController* multi_profile_user_controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_FAKE_USER_MANAGER_H_
