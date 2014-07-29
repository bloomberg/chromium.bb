// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_MOCK_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_MOCK_USER_MANAGER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chrome/browser/chromeos/login/users/avatar/mock_user_image_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class FakeSupervisedUserManager;

class MockUserManager : public UserManager {
 public:
  MockUserManager();
  virtual ~MockUserManager();

  MOCK_METHOD0(Shutdown, void(void));
  MOCK_CONST_METHOD0(GetUsersAdmittedForMultiProfile,
                     user_manager::UserList(void));
  MOCK_CONST_METHOD0(GetLoggedInUsers, const user_manager::UserList&(void));
  MOCK_CONST_METHOD0(GetLRULoggedInUsers, const user_manager::UserList&(void));
  MOCK_METHOD3(UserLoggedIn, void(
      const std::string&, const std::string&, bool));
  MOCK_METHOD1(SwitchActiveUser, void(const std::string& email));
  MOCK_METHOD0(SessionStarted, void(void));
  MOCK_METHOD2(RemoveUser, void(const std::string&, RemoveUserDelegate*));
  MOCK_METHOD1(RemoveUserFromList, void(const std::string&));
  MOCK_CONST_METHOD1(IsKnownUser, bool(const std::string&));
  MOCK_CONST_METHOD1(FindUser, const user_manager::User*(const std::string&));
  MOCK_METHOD1(FindUserAndModify, user_manager::User*(const std::string&));
  MOCK_METHOD2(SaveUserOAuthStatus,
               void(const std::string&, user_manager::User::OAuthTokenStatus));
  MOCK_METHOD2(SaveForceOnlineSignin, void(const std::string&, bool));
  MOCK_METHOD2(SaveUserDisplayName, void(const std::string&,
                                         const base::string16&));
  MOCK_METHOD2(UpdateUserAccountData,
               void(const std::string&, const UserAccountData&));
  MOCK_CONST_METHOD1(GetUserDisplayName, base::string16(const std::string&));
  MOCK_METHOD2(SaveUserDisplayEmail, void(const std::string&,
                                          const std::string&));
  MOCK_CONST_METHOD1(GetUserDisplayEmail, std::string(const std::string&));
  MOCK_CONST_METHOD0(IsCurrentUserOwner, bool(void));
  MOCK_CONST_METHOD0(IsCurrentUserNew, bool(void));
  MOCK_CONST_METHOD0(IsCurrentUserNonCryptohomeDataEphemeral, bool(void));
  MOCK_CONST_METHOD0(CanCurrentUserLock, bool(void));
  MOCK_CONST_METHOD0(IsUserLoggedIn, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsRegularUser, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsDemoUser, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsPublicAccount, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsGuest, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsSupervisedUser, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsKioskApp, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsStub, bool(void));
  MOCK_CONST_METHOD0(IsSessionStarted, bool(void));
  MOCK_CONST_METHOD1(IsUserNonCryptohomeDataEphemeral,
                     bool(const std::string&));
  MOCK_METHOD1(AddObserver, void(UserManager::Observer*));
  MOCK_METHOD1(RemoveObserver, void(UserManager::Observer*));
  MOCK_METHOD1(AddSessionStateObserver,
               void(UserManager::UserSessionStateObserver*));
  MOCK_METHOD1(RemoveSessionStateObserver,
               void(UserManager::UserSessionStateObserver*));
  MOCK_METHOD0(NotifyLocalStateChanged, void(void));
  MOCK_METHOD2(SetUserFlow, void(const std::string&, UserFlow*));
  MOCK_METHOD1(ResetUserFlow, void(const std::string&));

  MOCK_CONST_METHOD0(AreSupervisedUsersAllowed, bool(void));

  // You can't mock these functions easily because nobody can create
  // User objects but the ChromeUserManager and us.
  virtual const user_manager::UserList& GetUsers() const OVERRIDE;
  virtual const user_manager::User* GetLoggedInUser() const OVERRIDE;
  virtual user_manager::UserList GetUnlockUsers() const OVERRIDE;
  virtual const std::string& GetOwnerEmail() const OVERRIDE;
  virtual user_manager::User* GetLoggedInUser() OVERRIDE;
  virtual const user_manager::User* GetActiveUser() const OVERRIDE;
  virtual user_manager::User* GetActiveUser() OVERRIDE;
  virtual const user_manager::User* GetPrimaryUser() const OVERRIDE;

  virtual MultiProfileUserController* GetMultiProfileUserController() OVERRIDE;
  virtual UserImageManager* GetUserImageManager(
      const std::string& user_id) OVERRIDE;
  virtual SupervisedUserManager* GetSupervisedUserManager() OVERRIDE;

  virtual UserFlow* GetCurrentUserFlow() const OVERRIDE;
  virtual UserFlow* GetUserFlow(const std::string&) const OVERRIDE;

  // Sets a new User instance. Users previously created by this MockUserManager
  // become invalid.
  void SetActiveUser(const std::string& email);

  // Creates a new public session user. Users previously created by this
  // MockUserManager become invalid.
  user_manager::User* CreatePublicAccountUser(const std::string& email);

  // Adds a new User instance to the back of the user list. Users previously
  // created by this MockUserManager remain valid.
  void AddUser(const std::string& email);

  // Clears the user list and the active user. Users previously created by this
  // MockUserManager become invalid.
  void ClearUserList();

  scoped_ptr<UserFlow> user_flow_;
  scoped_ptr<MockUserImageManager> user_image_manager_;
  scoped_ptr<FakeSupervisedUserManager> supervised_user_manager_;
  user_manager::UserList user_list_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_MOCK_USER_MANAGER_H_
