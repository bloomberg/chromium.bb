// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_USER_MANAGER_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockUserManager : public UserManager {
 public:
  MockUserManager();
  virtual ~MockUserManager();

  MOCK_CONST_METHOD0(GetUsers, const UserList&(void));
  MOCK_METHOD1(UserLoggedIn, void(const std::string&));
  MOCK_METHOD0(DemoUserLoggedIn, void(void));
  MOCK_METHOD0(GuestUserLoggedIn, void(void));
  MOCK_METHOD1(EphemeralUserLoggedIn, void(const std::string&));
  MOCK_METHOD1(UserSelected, void(const std::string&));
  MOCK_METHOD0(SessionStarted, void(void));
  MOCK_METHOD2(RemoveUser, void(const std::string&, RemoveUserDelegate*));
  MOCK_METHOD1(RemoveUserFromList, void(const std::string&));
  MOCK_CONST_METHOD1(IsKnownUser, bool(const std::string&));
  MOCK_CONST_METHOD1(FindUser, const User*(const std::string&));
  MOCK_CONST_METHOD1(IsDisplayNameUnique, bool(const std::string&));
  MOCK_METHOD2(SaveUserOAuthStatus, void(const std::string&,
                                         User::OAuthTokenStatus));
  MOCK_METHOD2(SaveUserDisplayEmail, void(const std::string&,
                                          const std::string&));
  MOCK_METHOD2(GetLoggedInUserWallpaperProperties, void(User::WallpaperType*,
                                                        int*));
  MOCK_METHOD2(SaveLoggedInUserWallpaperProperties, void(User::WallpaperType,
                                                         int));
  MOCK_CONST_METHOD1(GetUserDisplayEmail, std::string(const std::string&));
  MOCK_METHOD2(SaveUserDefaultImageIndex, void(const std::string&, int));
  MOCK_METHOD2(SaveUserImage, void(const std::string&, const SkBitmap&));
  MOCK_METHOD1(SetLoggedInUserCustomWallpaperLayout,void(
      ash::WallpaperLayout));
  MOCK_METHOD2(SaveUserImageFromFile, void(const std::string&,
                                           const FilePath&));
  MOCK_METHOD4(SaveUserWallpaperFromFile, void(const std::string&,
                                               const FilePath&,
                                               ash::WallpaperLayout,
                                               WallpaperDelegate*));
  MOCK_METHOD1(SaveUserImageFromProfileImage, void(const std::string&));
  MOCK_METHOD1(DownloadProfileImage, void(const std::string&));
  MOCK_CONST_METHOD0(IsCurrentUserOwner, bool(void));
  MOCK_CONST_METHOD0(IsCurrentUserNew, bool(void));
  MOCK_CONST_METHOD0(IsCurrentUserEphemeral, bool(void));
  MOCK_CONST_METHOD0(IsUserLoggedIn, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsDemoUser, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsGuest, bool(void));
  MOCK_CONST_METHOD0(IsLoggedInAsStub, bool(void));
  MOCK_CONST_METHOD0(IsSessionStarted, bool(void));
  MOCK_METHOD1(AddObserver, void(UserManager::Observer*));
  MOCK_METHOD1(RemoveObserver, void(UserManager::Observer*));
  MOCK_METHOD0(NotifyLocalStateChanged, void(void));
  MOCK_CONST_METHOD0(DownloadedProfileImage, const SkBitmap& (void));

  // You can't mock this function easily because nobody can create User objects
  // but the UserManagerImpl and us.
  virtual const User& GetLoggedInUser() const OVERRIDE;

  // You can't mock this function easily because nobody can create User objects
  // but the UserManagerImpl and us.
  virtual User& GetLoggedInUser() OVERRIDE;

  // Sets a new User instance.
  void SetLoggedInUser(const std::string& email, bool guest);

  // Return an invalid user wallpaper index. No need to load a real wallpaper.
  // When loading wallpaper asynchronously, it may actually cause a crash if
  // test finished before wallpaper loaded.
  virtual int GetLoggedInUserWallpaperIndex() OVERRIDE;

  User* user_;
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
