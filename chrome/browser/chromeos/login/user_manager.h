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
#include "base/ref_counted.h"
#include "chrome/browser/chromeos/login/user_image_loader.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"

class FilePath;
class PrefService;

namespace chromeos {
class RemoveUserDelegate;

// This class provides a mechanism for discovering users who have logged
// into this chromium os device before and updating that list.
class UserManager : public UserImageLoader::Delegate,
                    public NotificationObserver {
 public:
  // A class representing information about a previously logged in user.
  class User {
   public:
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

    // The image for this user.
    void set_image(const SkBitmap& image) { image_ = image; }
    const SkBitmap& image() const { return image_; }

   private:
    std::string email_;
    SkBitmap image_;
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

  // Sets image for logged-in user and sends LOGIN_USER_IMAGE_CHANGED
  // notification about the image changed via NotificationService.
  void SetLoggedInUserImage(const SkBitmap& image);

  // Tries to load logged-in user image from disk and sets it for the user.
  void LoadLoggedInUserImage(const FilePath& path);

  // Saves image to file and saves image path in local state preferences.
  void SaveUserImage(const std::string& username,
                     const SkBitmap& image);

  // chromeos::UserImageLoader::Delegate implementation.
  virtual void OnImageLoaded(const std::string& username,
                             const SkBitmap& image,
                             bool save_image);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
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

  // Returns true if we're logged in as a Guest.
  bool IsLoggedInAsGuest() const;

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

  // Loads user image from its file.
  scoped_refptr<UserImageLoader> image_loader_;

  // Cache for user images. Stores image for each username.
  typedef base::hash_map<std::string, SkBitmap> UserImages;
  mutable UserImages user_images_;

  // The logged-in user.
  User logged_in_user_;

  // Cached flag of whether currently logged-in user is owner or not.
  bool current_user_is_owner_;

  // Cached flag of whether the currently logged-in user existed before this
  // login.
  bool current_user_is_new_;

  // Cached flag of whether any user is logged in at the moment.
  bool user_is_logged_in_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(UserManager);
};

typedef std::vector<UserManager::User> UserVector;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
