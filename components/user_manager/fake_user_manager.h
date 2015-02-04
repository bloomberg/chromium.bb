// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_
#define COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_base.h"

namespace user_manager {

// Fake user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class USER_MANAGER_EXPORT FakeUserManager : public UserManagerBase {
 public:
  FakeUserManager();
  ~FakeUserManager() override;

  // Create and add a new user.
  virtual const user_manager::User* AddUser(const std::string& email);

  // Calculates the user name hash and calls UserLoggedIn to login a user.
  void LoginUser(const std::string& email);

  // UserManager overrides.
  const user_manager::UserList& GetUsers() const override;
  user_manager::UserList GetUsersAllowedForMultiProfile() const override;
  const user_manager::UserList& GetLoggedInUsers() const override;

  // Set the user as logged in.
  void UserLoggedIn(const std::string& email,
                    const std::string& username_hash,
                    bool browser_restart) override;

  const user_manager::User* GetActiveUser() const override;
  user_manager::User* GetActiveUser() override;
  void SwitchActiveUser(const std::string& email) override;
  void SaveUserDisplayName(const std::string& username,
                           const base::string16& display_name) override;

  // Not implemented.
  void UpdateUserAccountData(const std::string& user_id,
                             const UserAccountData& account_data) override {}
  void Shutdown() override {}
  const user_manager::UserList& GetLRULoggedInUsers() const override;
  user_manager::UserList GetUnlockUsers() const override;
  const std::string& GetOwnerEmail() const override;
  void SessionStarted() override {}
  void RemoveUser(const std::string& email,
                  user_manager::RemoveUserDelegate* delegate) override {}
  void RemoveUserFromList(const std::string& email) override;
  bool IsKnownUser(const std::string& email) const override;
  const user_manager::User* FindUser(const std::string& email) const override;
  user_manager::User* FindUserAndModify(const std::string& email) override;
  const user_manager::User* GetLoggedInUser() const override;
  user_manager::User* GetLoggedInUser() override;
  const user_manager::User* GetPrimaryUser() const override;
  void SaveUserOAuthStatus(
      const std::string& username,
      user_manager::User::OAuthTokenStatus oauth_token_status) override {}
  void SaveForceOnlineSignin(const std::string& user_id,
                             bool force_online_signin) override {}
  base::string16 GetUserDisplayName(const std::string& username) const override;
  void SaveUserDisplayEmail(const std::string& username,
                            const std::string& display_email) override {}
  std::string GetUserDisplayEmail(const std::string& username) const override;
  bool IsCurrentUserOwner() const override;
  bool IsCurrentUserNew() const override;
  bool IsCurrentUserNonCryptohomeDataEphemeral() const override;
  bool CanCurrentUserLock() const override;
  bool IsUserLoggedIn() const override;
  bool IsLoggedInAsUserWithGaiaAccount() const override;
  bool IsLoggedInAsPublicAccount() const override;
  bool IsLoggedInAsGuest() const override;
  bool IsLoggedInAsSupervisedUser() const override;
  bool IsLoggedInAsKioskApp() const override;
  bool IsLoggedInAsStub() const override;
  bool IsSessionStarted() const override;
  bool IsUserNonCryptohomeDataEphemeral(
      const std::string& email) const override;
  void AddObserver(Observer* obs) override {}
  void RemoveObserver(Observer* obs) override {}
  void AddSessionStateObserver(UserSessionStateObserver* obs) override {}
  void RemoveSessionStateObserver(UserSessionStateObserver* obs) override {}
  void NotifyLocalStateChanged() override {}
  bool AreSupervisedUsersAllowed() const override;

  // UserManagerBase overrides:
  bool AreEphemeralUsersEnabled() const override;
  const std::string& GetApplicationLocale() const override;
  PrefService* GetLocalState() const override;
  void HandleUserOAuthTokenStatusChange(
      const std::string& user_id,
      user_manager::User::OAuthTokenStatus status) const override {}
  bool IsEnterpriseManaged() const override;
  void LoadPublicAccounts(std::set<std::string>* public_sessions_set) override {
  }
  void PerformPreUserListLoadingActions() override {}
  void PerformPostUserListLoadingActions() override {}
  void PerformPostUserLoggedInActions(bool browser_restart) override {}
  bool IsDemoApp(const std::string& user_id) const override;
  bool IsKioskApp(const std::string& user_id) const override;
  bool IsPublicAccountMarkedForRemoval(
      const std::string& user_id) const override;
  void DemoAccountLoggedIn() override {}
  void KioskAppLoggedIn(const std::string& app_id) override {}
  void PublicAccountUserLoggedIn(user_manager::User* user) override {}
  void SupervisedUserLoggedIn(const std::string& user_id) override {}

 protected:
  user_manager::User* primary_user_;

  // If set this is the active user. If empty, the first created user is the
  // active user.
  std::string active_user_id_;

 private:
  // We use this internal function for const-correctness.
  user_manager::User* GetActiveUserInternal() const;

  // stub, always empty string.
  std::string owner_email_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserManager);
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_FAKE_USER_MANAGER_H_
