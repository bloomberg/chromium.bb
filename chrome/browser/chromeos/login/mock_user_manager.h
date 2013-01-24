// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_MANAGER_H_

#include <string>

#include "base/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/mock_user_image_manager.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockUserManager : public UserManager {
 public:
  MockUserManager();
  virtual ~MockUserManager();

  MOCK_METHOD0(Shutdown, void(void));
  MOCK_CONST_METHOD0(GetUsers, const UserList&(void));
  MOCK_METHOD2(UserLoggedIn, void(const std::string&, bool));
  MOCK_METHOD0(RetailModeUserLoggedIn, void(void));
  MOCK_METHOD0(GuestUserLoggedIn, void(void));
  MOCK_METHOD1(LocallyManagedUserLoggedIn, void(const std::string&));
  MOCK_METHOD1(PublicAccountUserLoggedIn, void(User*));
  MOCK_METHOD2(RegularUserLoggedIn, void(const std::string&, bool));
  MOCK_METHOD1(RegularUserLoggedInAsEphemeral, void(const std::string&));
  MOCK_METHOD0(SessionStarted, void(void));
  MOCK_METHOD1(CreateLocallyManagedUserRecord,
               const User*(const string16& display_name));
  MOCK_METHOD2(RemoveUser, void(const std::string&, RemoveUserDelegate*));
  MOCK_METHOD1(RemoveUserFromList, void(const std::string&));
  MOCK_CONST_METHOD1(IsKnownUser, bool(const std::string&));
  MOCK_CONST_METHOD1(FindUser, const User*(const std::string&));
  MOCK_CONST_METHOD1(FindLocallyManagedUser, const User*(const string16&));
  MOCK_METHOD2(SaveUserOAuthStatus, void(const std::string&,
                                         User::OAuthTokenStatus));
  MOCK_METHOD2(SaveUserDisplayName, void(const std::string&,
                                         const string16&));
  MOCK_CONST_METHOD1(GetUserDisplayName, string16(const std::string&));
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
  MOCK_CONST_METHOD0(IsLoggedInAsLocallyManagedUser, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsStub, bool(void));
  MOCK_CONST_METHOD0(IsSessionStarted, bool(void));
  MOCK_CONST_METHOD0(HasBrowserRestarted, bool(void));
  MOCK_CONST_METHOD1(IsUserNonCryptohomeDataEphemeral,
                     bool(const std::string&));
  MOCK_METHOD1(AddObserver, void(UserManager::Observer*));
  MOCK_METHOD1(RemoveObserver, void(UserManager::Observer*));
  MOCK_METHOD0(NotifyLocalStateChanged, void(void));
  MOCK_METHOD0(CreateLocallyManagedUserRecord, void(void));

  // You can't mock this function easily because nobody can create User objects
  // but the UserManagerImpl and us.
  virtual const User* GetLoggedInUser() const OVERRIDE;

  // You can't mock this function easily because nobody can create User objects
  // but the UserManagerImpl and us.
  virtual User* GetLoggedInUser() OVERRIDE;

  virtual UserImageManager* GetUserImageManager() OVERRIDE;

  // Sets a new User instance.
  void SetLoggedInUser(const std::string& email);

  User* user_;
  scoped_ptr<MockUserImageManager> user_image_manager_;
};

// Class that provides easy life-cycle management for mocking the UserManager
// for tests.
class ScopedMockUserManagerEnabler {
 public:
  ScopedMockUserManagerEnabler();
  ~ScopedMockUserManagerEnabler();

  MockUserManager* user_manager();

 private:
  UserManager* old_user_manager_;
  scoped_ptr<MockUserManager> user_manager_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_MANAGER_H_
