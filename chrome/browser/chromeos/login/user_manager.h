// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "chrome/browser/chromeos/login/profile_image_downloader.h"
#include "chrome/browser/chromeos/login/user_image_loader.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"

class FilePath;
class PrefService;

namespace base {
template<typename> struct DefaultLazyInstanceTraits;
}

namespace chromeos {
class RemoveUserDelegate;

// This class provides a mechanism for discovering users who have logged
// into this chromium os device before and updating that list.
class UserManager : public UserImageLoader::Delegate,
                    public ProfileImageDownloader::Delegate,
                    public NotificationObserver {
 public:
  // User OAuth token status according to the last check.
  typedef enum {
    OAUTH_TOKEN_STATUS_UNKNOWN = 0,
    OAUTH_TOKEN_STATUS_INVALID = 1,
    OAUTH_TOKEN_STATUS_VALID   = 2,
  } OAuthTokenStatus;

  // A class representing information about a previously logged in user.
  class User {
   public:
    // Returned as default_image_index when user-selected file or photo
    // is used as user image.
    static const int kExternalImageIndex = -1;
    // Returned as default_image_index when user profile image is used as
    // user image.
    static const int kProfileImageIndex = -2;
    static const int kInvalidImageIndex = -3;

    User();
    ~User();

    // The email the user used to log in.
    void set_email(const std::string& email) { email_ = email; }
    const std::string& email() const { return email_; }

    // Returns the name to display for this user.
    std::string GetDisplayName() const;

    // Tooltip contains user's display name and his email domain to distinguish
    // this user from the other one with the same display name.
    std::string GetNameTooltip() const;

    // Returns true if some users have same display name.
    bool NeedsNameTooltip() const;

    // The image for this user.
    void SetImage(const SkBitmap& image,
                  int default_image_index);
    const SkBitmap& image() const { return image_; }
    int default_image_index() const { return default_image_index_; }

    // OAuth token status for this user.
    OAuthTokenStatus oauth_token_status() const { return oauth_token_status_; }
    void set_oauth_token_status(OAuthTokenStatus status) {
      oauth_token_status_ = status;
    }

   private:
    friend class UserManager;

    std::string email_;
    SkBitmap image_;
    OAuthTokenStatus oauth_token_status_;

    // Cached flag of whether any users has same display name.
    bool is_displayname_unique_;

    // Index of the default image the user has set. |kExternalImageIndex|
    // if it's some other image.
    int default_image_index_;
  };

  // Gets a shared instance of a UserManager. Not thread-safe...should
  // only be called from the main UI thread.
  static UserManager* Get();

  // Registers user manager preferences.
  static void RegisterPrefs(PrefService* local_state);

  // Returns a list of the users who have logged into this device previously.
  // It is sorted in order of recency, with most recent at the beginning.
  virtual std::vector<User> GetUsers() const;

  // Indicates that user just started incognito session.
  virtual void OffTheRecordUserLoggedIn();

  // Indicates that a user with the given email has just logged in.
  // The persistent list will be updated accordingly.
  virtual void UserLoggedIn(const std::string& email);

  // Removes the user from the device. Note, it will verify that the given user
  // isn't the owner, so calling this method for the owner will take no effect.
  // Note, |delegate| can be NULL.
  virtual void RemoveUser(const std::string& email,
                          RemoveUserDelegate* delegate);

  // Removes the user from the persistent list only. Also removes the user's
  // picture.
  virtual void RemoveUserFromList(const std::string& email);

  // Returns true if given user has logged into the device before.
  virtual bool IsKnownUser(const std::string& email);

  // Returns the logged-in user.
  virtual const User& logged_in_user() const;

  // Sets image for logged-in user.
  void SetLoggedInUserImage(const SkBitmap& image, int default_image_index);

  // Tries to load logged-in user image from disk and sets it for the user.
  void LoadLoggedInUserImage(const FilePath& path);

  // Saves image to file, saves image path in local state preferences
  // and sends LOGIN_USER_IMAGE_CHANGED notification about the image
  // changed via NotificationService.
  void SaveUserImage(const std::string& username,
                     const SkBitmap& image,
                     int image_index);

  // Saves user's oauth token status in local state preferences.
  void SaveUserOAuthStatus(const std::string& username,
                           OAuthTokenStatus oauth_token_status);

  // Gets user's oauth token status in local state preferences.
  OAuthTokenStatus GetUserOAuthStatus(const std::string& username);

  // Saves user image path for the user. Can be used to set default images.
  // Sends LOGIN_USER_IMAGE_CHANGED notification about the image changed
  // via NotificationService.
  void SaveUserImagePath(const std::string& username,
                         const std::string& image_path,
                         int image_index);

  // Returns the index of user's default image or |kInvalidImageIndex|
  // if some error occurs (like Local State corruption).
  int GetUserDefaultImageIndex(const std::string& username);

  // chromeos::UserImageLoader::Delegate implementation.
  virtual void OnImageLoaded(const std::string& username,
                             const SkBitmap& image,
                             int image_index,
                             bool save_image);

  // ProfileImageDownloader::Delegate implementation.
  virtual void OnDownloadSuccess(const SkBitmap& image) OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Accessor for current_user_is_owner_
  virtual bool current_user_is_owner() const;
  virtual void set_current_user_is_owner(bool current_user_is_owner);

  // Accessor for current_user_is_new_.
  bool current_user_is_new() const {
    return current_user_is_new_;
  }

  bool user_is_logged_in() const { return user_is_logged_in_; }

  void set_offline_login(bool value) { offline_login_ = value; }
  bool offline_login() { return offline_login_; }

  // Returns true if we're logged in as a Guest.
  bool IsLoggedInAsGuest() const;

  // Interface that observers of UserManager must implement in order
  // to receive notification when local state preferences is changed
  class Observer {
   public:
    // Called when the local state preferences is changed
    virtual void LocalStateChanged(UserManager* user_manager) = 0;

   protected:
    virtual ~Observer() {}
  };

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  void NotifyLocalStateChanged();

  // Starts downloading the profile image for the logged-in user.
  // Used to post a delayed task for that.
  void DownloadProfileImage();

 protected:
  UserManager();
  virtual ~UserManager();

  // Returns image filepath for the given user.
  FilePath GetImagePathForUser(const std::string& username);

 private:
  // Notifies on new user session.
  void NotifyOnLogin();

  // Sets one of the default images to the specified user and saves this
  // setting in local state.
  void SetDefaultUserImage(const std::string& username);

  // Sets image for user |username|.
  void SetUserImage(const std::string& username,
                    const SkBitmap& image,
                    int default_image_index,
                    bool save_image);

  // Loads user image from its file.
  scoped_refptr<UserImageLoader> image_loader_;

  // Cache for user images. Stores image for each username.
  typedef base::hash_map<std::string, SkBitmap> UserImages;
  mutable UserImages user_images_;

  // The logged-in user.
  User logged_in_user_;

  // Current user is logged in offline. Valid only for WebUI login flow.
  bool offline_login_;

  // Cached flag of whether currently logged-in user is owner or not.
  // May be accessed on different threads, requires locking.
  bool current_user_is_owner_;
  mutable base::Lock current_user_is_owner_lock_;

  // Cached flag of whether the currently logged-in user existed before this
  // login.
  bool current_user_is_new_;

  // Cached flag of whether any user is logged in at the moment.
  bool user_is_logged_in_;

  NotificationRegistrar registrar_;

  friend struct base::DefaultLazyInstanceTraits<UserManager>;

  ObserverList<Observer> observer_list_;

  // Download user profile image on login to update it if it's changed.
  scoped_ptr<ProfileImageDownloader> profile_image_downloader_;

  ScopedRunnableMethodFactory<UserManager> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(UserManager);
};

typedef std::vector<UserManager::User> UserVector;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
