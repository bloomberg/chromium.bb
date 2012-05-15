// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
#pragma once

#include <string>

#include "ash/desktop_background/desktop_background_resources.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/login/user.h"

class SkBitmap;
class FilePath;
class PrefService;

namespace chromeos {

class RemoveUserDelegate;

// Base class for UserManagerImpl - provides a mechanism for discovering users
// who have logged into this Chrome OS device before and updating that list.
class UserManager {
 public:
  // Interface that observers of UserManager must implement in order
  // to receive notification when local state preferences is changed
  class Observer {
   public:
    // Called when the local state preferences is changed
    virtual void LocalStateChanged(UserManager* user_manager) = 0;

   protected:
    virtual ~Observer() {}
  };

  // A vector pref of the users who have logged into the device.
  static const char kLoggedInUsers[];

  // A dictionary that maps usernames to file paths to their wallpapers.
  // Deprecated. Will remove this const char after done migration.
  static const char kUserWallpapers[];

  // A dictionary that maps usernames to wallpaper properties.
  static const char kUserWallpapersProperties[];

  // A dictionary that maps usernames to file paths to their images.
  static const char kUserImages[];

  // A dictionary that maps usernames to the displayed (non-canonical) emails.
  static const char kUserDisplayEmail[];

  // A dictionary that maps usernames to OAuth token presence flag.
  static const char kUserOAuthTokenStatus[];

  // Returns a shared instance of a UserManager. Not thread-safe, should only be
  // called from the main UI thread.
  static UserManager* Get();

  // Set UserManager singleton object for test purpose only! Returns the
  // previous singleton object and releases it from the singleton memory
  // management. It is the responsibility of the test writer to restore the
  // original object or delete it if needed.
  //
  // The intended usage is meant to be something like this:
  //   virtual void SetUp() {
  //     mock_user_manager_.reset(new MockUserManager());
  //     old_user_manager_ = UserManager::Set(mock_user_manager_.get());
  //     EXPECT_CALL...
  //     ...
  //   }
  //   virtual void TearDown() {
  //     ...
  //     UserManager::Set(old_user_manager_);
  //   }
  //   scoped_ptr<MockUserManager> mock_user_manager_;
  //   UserManager* old_user_manager_;
  static UserManager* Set(UserManager* mock);

  // Registers user manager preferences.
  static void RegisterPrefs(PrefService* local_state);

  virtual ~UserManager();

  // Returns a list of users who have logged into this device previously. This
  // is sorted by last login date with the most recent user at the beginning.
  virtual const UserList& GetUsers() const = 0;

  // Indicates that a user with the given email has just logged in. The
  // persistent list is updated accordingly if the user is not ephemeral.
  virtual void UserLoggedIn(const std::string& email) = 0;

  // Indicates that user just logged on as the demo user.
  virtual void DemoUserLoggedIn() = 0;

  // Indicates that user just started incognito session.
  virtual void GuestUserLoggedIn() = 0;

  // Indicates that a user just logged in as ephemeral.
  virtual void EphemeralUserLoggedIn(const std::string& email) = 0;

  // Called when user pod with |email| is selected.
  virtual void UserSelected(const std::string& email) = 0;

  // Called when browser session is started i.e. after
  // browser_creator.LaunchBrowser(...) was called after user sign in.
  // When user is at the image screen IsUserLoggedIn() will return true
  // but SessionStarted() will return false.
  // Fires NOTIFICATION_SESSION_STARTED.
  virtual void SessionStarted() = 0;

  // Removes the user from the device. Note, it will verify that the given user
  // isn't the owner, so calling this method for the owner will take no effect.
  // Note, |delegate| can be NULL.
  virtual void RemoveUser(const std::string& email,
                          RemoveUserDelegate* delegate) = 0;

  // Removes the user from the persistent list only. Also removes the user's
  // picture.
  virtual void RemoveUserFromList(const std::string& email) = 0;

  // Returns true if a user with the given email address is found in the
  // persistent list or currently logged in as ephemeral.
  virtual bool IsKnownUser(const std::string& email) const = 0;

  // Returns the user with the given email address if found in the persistent
  // list or currently logged in as ephemeral. Returns |NULL| otherwise.
  virtual const User* FindUser(const std::string& email) const = 0;

  // Returns the logged-in user.
  virtual const User& GetLoggedInUser() const = 0;
  virtual User& GetLoggedInUser() = 0;

  // Returns true if given display name is unique.
  virtual bool IsDisplayNameUnique(const std::string& display_name) const = 0;

  // Saves user's oauth token status in local state preferences.
  virtual void SaveUserOAuthStatus(
      const std::string& username,
      User::OAuthTokenStatus oauth_token_status) = 0;

  // Save user's displayed (non-canonical) email in local state preferences.
  // Ignored If there is no such user.
  virtual void SaveUserDisplayEmail(const std::string& username,
                                    const std::string& display_email) = 0;

  // Returns the display email for user |username| if it is known (was
  // previously set by a |SaveUserDisplayEmail| call).
  // Otherwise, returns |username| itself.
  virtual std::string GetUserDisplayEmail(
      const std::string& username) const = 0;

  // Returns the index of the default wallpapers saved in local state for login
  // user if it is known (was previousely set by |SaveWallpaperToLocalState|
  // call). Otherwise, returns the default wallpaper index.
  virtual int GetLoggedInUserWallpaperIndex() = 0;

  // Set |type| and |index| to the value saved in local state for logged in
  // user.
  virtual void GetLoggedInUserWallpaperProperties(User::WallpaperType* type,
                                                  int* index) = 0;

  // Save |type| and |index| chose by logged in user to Local State.
  virtual void SaveLoggedInUserWallpaperProperties(User::WallpaperType type,
                                                   int index) = 0;

  // Sets user image to the default image with index |image_index|, sends
  // LOGIN_USER_IMAGE_CHANGED notification and updates Local State.
  virtual void SaveUserDefaultImageIndex(const std::string& username,
                                         int image_index) = 0;

  // Saves image to file, sends LOGIN_USER_IMAGE_CHANGED notification and
  // updates Local State.
  virtual void SaveUserImage(const std::string& username,
                             const SkBitmap& image) = 0;

  // Tries to load user image from disk; if successful, sets it for the user,
  // sends LOGIN_USER_IMAGE_CHANGED notification and updates Local State.
  virtual void SaveUserImageFromFile(const std::string& username,
                                     const FilePath& path) = 0;

  // Sets profile image as user image for |username|, sends
  // LOGIN_USER_IMAGE_CHANGED notification and updates Local State. If the user
  // is not logged-in or the last |DownloadProfileImage| call has failed, a
  // default grey avatar will be used until the user logs in and profile image
  // is downloaded successfuly.
  virtual void SaveUserImageFromProfileImage(const std::string& username) = 0;

  // Starts downloading the profile image for the logged-in user.
  // If user's image index is |kProfileImageIndex|, newly downloaded image
  // is immediately set as user's current picture.
  // |reason| is an arbitraty string (used to report UMA histograms with
  // download times).
  virtual void DownloadProfileImage(const std::string& reason) = 0;

  // Returns true if current user is an owner.
  virtual bool IsCurrentUserOwner() const = 0;

  // Returns true if current user is not existing one (hasn't signed in before).
  virtual bool IsCurrentUserNew() const = 0;

  // Returns true if the current user is ephemeral.
  virtual bool IsCurrentUserEphemeral() const = 0;

  // Returns true if user is signed in.
  virtual bool IsUserLoggedIn() const = 0;

  // Returns true if we're logged in as a demo user.
  virtual bool IsLoggedInAsDemoUser() const = 0;

  // Returns true if we're logged in as a Guest.
  virtual bool IsLoggedInAsGuest() const = 0;

  // Returns true if we're logged in as the stub user used for testing on Linux.
  virtual bool IsLoggedInAsStub() const = 0;

  // Returns true if we're logged in and browser has been started i.e.
  // browser_creator.LaunchBrowser(...) was called after sign in
  // or restart after crash.
  virtual bool IsSessionStarted() const = 0;

  virtual void AddObserver(Observer* obs) = 0;
  virtual void RemoveObserver(Observer* obs) = 0;

  virtual void NotifyLocalStateChanged() = 0;

  // Returns the result of the last successful profile image download, if any.
  // Otherwise, returns an empty bitmap.
  virtual const SkBitmap& DownloadedProfileImage() const = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
