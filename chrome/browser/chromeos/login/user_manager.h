// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image_loader.h"
#include "chrome/browser/profiles/profile_downloader_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"

class FilePath;
class PrefService;
class ProfileDownloader;

namespace base {
template<typename> struct DefaultLazyInstanceTraits;
}

namespace chromeos {

class RemoveUserDelegate;

// This class provides a mechanism for discovering users who have logged
// into this chromium os device before and updating that list.
class UserManager : public ProfileDownloaderDelegate,
                    public content::NotificationObserver {
 public:
  // Returns a shared instance of a UserManager. Not thread-safe, should only be
  // called from the main UI thread.
  static UserManager* Get();

  // Registers user manager preferences.
  static void RegisterPrefs(PrefService* local_state);

  // Returns a list of the users who have logged into this device previously.
  // It is sorted in order of recency, with most recent at the beginning.
  const UserList& GetUsers() const;

  // Indicates that a user with the given email has just logged in.
  // The persistent list will be updated accordingly.
  void UserLoggedIn(const std::string& email);

  // Indicates that user just started incognito session.
  void GuestUserLoggedIn();

  // Removes the user from the device. Note, it will verify that the given user
  // isn't the owner, so calling this method for the owner will take no effect.
  // Note, |delegate| can be NULL.
  void RemoveUser(const std::string& email,
                  RemoveUserDelegate* delegate);

  // Removes the user from the persistent list only. Also removes the user's
  // picture.
  void RemoveUserFromList(const std::string& email);

  // Returns true if given user has logged into the device before.
  virtual bool IsKnownUser(const std::string& email) const;

  // Returns a user with given email or |NULL| if no such user exists.
  const User* FindUser(const std::string& email) const;

  // Returns the logged-in user.
  const User& logged_in_user() const { return *logged_in_user_; }
  User& logged_in_user() { return *logged_in_user_; }

  // Returns true if given display name is unique.
  bool IsDisplayNameUnique(const std::string& display_name) const;

  // Saves user's oauth token status in local state preferences.
  void SaveUserOAuthStatus(const std::string& username,
                           User::OAuthTokenStatus oauth_token_status);

  // Save user's displayed (non-canonical) email in local state preferences.
  // Ignored If there is no such user.
  void SaveUserDisplayEmail(const std::string& username,
                            const std::string& display_email);

  // Returns the display email for user |username| if it is known (was
  // previously set by a |SaveUserDisplayEmail| call).
  // Otherwise, returns |username| itself.
  std::string GetUserDisplayEmail(const std::string& username) const;

  // Sets user image to the default image with index |image_index|, sends
  // LOGIN_USER_IMAGE_CHANGED notification and updates Local State.
  void SaveUserDefaultImageIndex(const std::string& username, int image_index);

  // Saves image to file, sends LOGIN_USER_IMAGE_CHANGED notification and
  // updates Local State.
  void SaveUserImage(const std::string& username, const SkBitmap& image);

  // Tries to load user image from disk; if successful, sets it for the user,
  // sends LOGIN_USER_IMAGE_CHANGED notification and updates Local State.
  void SaveUserImageFromFile(const std::string& username, const FilePath& path);

  // Sets profile image as user image for |username|, sends
  // LOGIN_USER_IMAGE_CHANGED notification and updates Local State. If the user
  // is not logged-in or the last |DownloadProfileImage| call has failed, a
  // default grey avatar will be used until the user logs in and profile image
  // is downloaded successfuly.
  void SaveUserImageFromProfileImage(const std::string& username);

  // Starts downloading the profile image for the logged-in user.
  // If user's image index is |kProfileImageIndex|, newly downloaded image
  // is immediately set as user's current picture.
  void DownloadProfileImage();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Accessor for current_user_is_owner_
  virtual bool current_user_is_owner() const;
  virtual void set_current_user_is_owner(bool current_user_is_owner);

  // Accessor for current_user_is_new_.
  bool current_user_is_new() const {
    return current_user_is_new_;
  }

  bool user_is_logged_in() const { return user_is_logged_in_; }

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

  // Returns the result of the last successful profile image download, if any.
  // Otherwise, returns an empty bitmap.
  const SkBitmap& downloaded_profile_image() const {
    return downloaded_profile_image_;
  }

 protected:
  UserManager();
  virtual ~UserManager();

  // Returns image filepath for the given user.
  FilePath GetImagePathForUser(const std::string& username);

 private:
  // Loads |users_| from Local State if the list has not been loaded yet.
  // Subsequent calls have no effect. Must be called on the UI thread.
  void EnsureUsersLoaded();

  // Makes stub user the current logged-in user (for test paths).
  void StubUserLoggedIn();

  // Notifies on new user session.
  void NotifyOnLogin();

  // Reads user's oauth token status from local state preferences.
  User::OAuthTokenStatus LoadUserOAuthStatus(const std::string& username) const;

  // Sets one of the default images for the specified user and saves this
  // setting in local state.
  // Does not send LOGIN_USER_IMAGE_CHANGED notification.
  void SetInitialUserImage(const std::string& username);

  // Sets image for user |username| and sends LOGIN_USER_IMAGE_CHANGED
  // notification unless this is a new user and image is set for the first time.
  void SetUserImage(const std::string& username,
                    int image_index,
                    const SkBitmap& image);

  // Saves image to file, updates local state preferences to given image index
  // and sends LOGIN_USER_IMAGE_CHANGED notification.
  void SaveUserImageInternal(const std::string& username,
                             int image_index,
                             const SkBitmap& image);

  // Saves image to file with specified path and sends LOGIN_USER_IMAGE_CHANGED
  // notification. Runs on FILE thread. Posts task for saving image info to
  // Local State on UI thread.
  void SaveImageToFile(const std::string& username,
                       const SkBitmap& image,
                       const FilePath& image_path,
                       int image_index);

  // Stores path to the image and its index in local state. Runs on UI thread.
  // If |is_async| is true, it has been posted from the FILE thread after
  // saving the image.
  void SaveImageToLocalState(const std::string& username,
                             const std::string& image_path,
                             int image_index,
                             bool is_async);

  // Deletes user's image file. Runs on FILE thread.
  void DeleteUserImage(const FilePath& image_path);

  // Updates current user ownership on UI thread.
  void UpdateOwnership(bool is_owner);

  // Checks current user's ownership on file thread.
  void CheckOwnership();

  // ProfileDownloaderDelegate implementation.
  virtual int GetDesiredImageSideLength() const OVERRIDE;
  virtual Profile* GetBrowserProfile() OVERRIDE;
  virtual std::string GetCachedPictureURL() const OVERRIDE;
  virtual void OnDownloadComplete(ProfileDownloader* downloader,
                                  bool success) OVERRIDE;

  // Creates a new User instance.
  User* CreateUser(const std::string& email) const;

  // Loads user image from its file.
  scoped_refptr<UserImageLoader> image_loader_;

  // List of all known users. User instances are owned by |this| and deleted
  // when a user is removed with |RemoveUser|.
  mutable UserList users_;

  // Map of users' display names used to determine which users have unique
  // display names.
  mutable base::hash_map<std::string, size_t> display_name_count_;

  // User instance used to represent the off-the-record (guest) user.
  User guest_user_;

  // A stub User instance for test paths (running without a logged-in user).
  User stub_user_;

  // The logged-in user. NULL until a user has logged in, then points to one
  // of the User instances in |users_| or to the |guest_user_| instance.
  // In test paths without login points to the |stub_user_| instance.
  User* logged_in_user_;

  // Cached flag of whether currently logged-in user is owner or not.
  // May be accessed on different threads, requires locking.
  bool current_user_is_owner_;
  mutable base::Lock current_user_is_owner_lock_;

  // Cached flag of whether the currently logged-in user existed before this
  // login.
  bool current_user_is_new_;

  // Cached flag of whether any user is logged in at the moment.
  bool user_is_logged_in_;

  content::NotificationRegistrar registrar_;

  friend struct base::DefaultLazyInstanceTraits<UserManager>;

  ObserverList<Observer> observer_list_;

  // Download user profile image on login to update it if it's changed.
  scoped_ptr<ProfileDownloader> profile_image_downloader_;

  // Time when the profile image download has started.
  base::Time profile_image_load_start_time_;

  // True if the last user image required async save operation (which may not
  // have been completed yet). This flag is used to avoid races when user image
  // is first set with |SaveUserImage| and then with |SaveUserImagePath|.
  bool last_image_set_async_;

  // Result of the last successful profile image download, if any.
  SkBitmap downloaded_profile_image_;

  // Data URL for |downloaded_profile_image_|.
  std::string downloaded_profile_image_data_url_;

  DISALLOW_COPY_AND_ASSIGN(UserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
