// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_FAKE_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_FAKE_USER_MANAGER_H_

#include <string>

#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_manager.h"

namespace chromeos {

// Fake user manager with a barebones implementation. Users can be added
// and set as logged in, and those users can be returned.
class FakeUserManager : public UserManager {
 public:
  FakeUserManager();
  virtual ~FakeUserManager();

  // Create and add a new user.
  void AddUser(const std::string& email);

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
  virtual void SaveUserDisplayName(const std::string& username,
      const string16& display_name) OVERRIDE;

  // Not implemented.
  virtual void Shutdown() OVERRIDE {}
  virtual UserImageManager* GetUserImageManager() OVERRIDE;
  virtual const UserList& GetLRULoggedInUsers() OVERRIDE;
  virtual UserList GetUnlockUsers() const OVERRIDE;
  virtual const std::string& GetOwnerEmail() OVERRIDE;
  virtual void SwitchActiveUser(const std::string& email) OVERRIDE {}
  virtual void SessionStarted() OVERRIDE {}
  virtual void RestoreActiveSessions() OVERRIDE {}
  virtual const User* CreateLocallyManagedUserRecord(
      const std::string& manager_id,
      const std::string& local_user_id,
      const std::string& sync_user_id,
      const string16& display_name) OVERRIDE;
  virtual std::string GenerateUniqueLocallyManagedUserId() OVERRIDE;
  virtual void RemoveUser(const std::string& email,
      RemoveUserDelegate* delegate) OVERRIDE {}
  virtual void RemoveUserFromList(const std::string& email) OVERRIDE {}
  virtual bool IsKnownUser(const std::string& email) const OVERRIDE;
  virtual const User* FindUser(const std::string& email) const OVERRIDE;
  virtual const User* FindLocallyManagedUser(
      const string16& display_name) const OVERRIDE;
  virtual const User* GetLoggedInUser() const OVERRIDE;
  virtual User* GetLoggedInUser() OVERRIDE;
  virtual const User* GetPrimaryUser() const OVERRIDE;
  virtual void SaveUserOAuthStatus(
      const std::string& username,
      User::OAuthTokenStatus oauth_token_status) OVERRIDE {}
  virtual string16 GetUserDisplayName(
      const std::string& username) const OVERRIDE;
  virtual void SaveUserDisplayEmail(const std::string& username,
      const std::string& display_email) OVERRIDE {}
  virtual std::string GetUserDisplayEmail(
      const std::string& username) const OVERRIDE;
  virtual std::string GetManagedUserSyncId(
      const std::string& managed_user_id) const OVERRIDE;
  virtual string16 GetManagerDisplayNameForManagedUser(
      const std::string& managed_user_id) const OVERRIDE;
  virtual std::string GetManagerUserIdForManagedUser(
      const std::string& managed_user_id) const OVERRIDE;
  virtual std::string GetManagerDisplayEmailForManagedUser(
      const std::string& managed_user_id) const OVERRIDE;
  virtual bool IsCurrentUserOwner() const OVERRIDE;
  virtual bool IsCurrentUserNew() const OVERRIDE;
  virtual bool IsCurrentUserNonCryptohomeDataEphemeral() const OVERRIDE;
  virtual bool CanCurrentUserLock() const OVERRIDE;
  virtual bool IsUserLoggedIn() const OVERRIDE;
  virtual bool IsLoggedInAsRegularUser() const OVERRIDE;
  virtual bool IsLoggedInAsDemoUser() const OVERRIDE;
  virtual bool IsLoggedInAsPublicAccount() const OVERRIDE;
  virtual bool IsLoggedInAsGuest() const OVERRIDE;
  virtual bool IsLoggedInAsLocallyManagedUser() const OVERRIDE;
  virtual bool IsLoggedInAsKioskApp() const OVERRIDE;
  virtual bool IsLoggedInAsStub() const OVERRIDE;
  virtual bool IsSessionStarted() const OVERRIDE;
  virtual bool UserSessionsRestored() const OVERRIDE;
  virtual bool HasBrowserRestarted() const OVERRIDE;
  virtual bool IsUserNonCryptohomeDataEphemeral(
      const std::string& email) const OVERRIDE;
  virtual void StartLocallyManagedUserCreationTransaction(
      const string16& display_name) OVERRIDE {}
  virtual void SetLocallyManagedUserCreationTransactionUserId(
      const std::string& email) OVERRIDE {}
  virtual void CommitLocallyManagedUserCreationTransaction() OVERRIDE {}
  virtual void SetUserFlow(const std::string& email, UserFlow* flow) OVERRIDE {}
  virtual UserFlow* GetCurrentUserFlow() const OVERRIDE;
  virtual UserFlow* GetUserFlow(const std::string& email) const OVERRIDE;
  virtual void ResetUserFlow(const std::string& email) OVERRIDE {}
  virtual bool GetAppModeChromeClientOAuthInfo(
      std::string* chrome_client_id,
      std::string* chrome_client_secret) OVERRIDE;
  virtual void SetAppModeChromeClientOAuthInfo(
      const std::string& chrome_client_id,
      const std::string& chrome_client_secret) OVERRIDE {}
  virtual void AddObserver(Observer* obs) OVERRIDE {}
  virtual void RemoveObserver(Observer* obs) OVERRIDE {}
  virtual void AddSessionStateObserver(
      UserSessionStateObserver* obs) OVERRIDE {}
  virtual void RemoveSessionStateObserver(
      UserSessionStateObserver* obs) OVERRIDE {}
  virtual void NotifyLocalStateChanged() OVERRIDE {}
  virtual bool AreLocallyManagedUsersAllowed() const OVERRIDE;
  virtual base::FilePath GetUserProfileDir(const std::string& email) const
      OVERRIDE;

 private:
  UserList user_list_;
  UserList logged_in_users_;
  std::string owner_email_;

  DISALLOW_COPY_AND_ASSIGN(FakeUserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_FAKE_USER_MANAGER_H_
