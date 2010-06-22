// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/ref_counted.h"
#include "chrome/browser/chromeos/login/user_image_loader.h"
#include "third_party/skia/include/core/SkBitmap.h"

class PrefService;

namespace chromeos {

// This class provides a mechanism for discovering users who have logged
// into this chromium os device before and updating that list.
class UserManager : public UserImageLoader::Delegate {
 public:
  // A class representing information about a previously logged in user.
  class User {
   public:
    User();
    ~User() {}

    // The email the user used to log in.
    void set_email(const std::string& email) { email_ = email; }
    const std::string& email() const { return email_; }

    // Returns the name to display for this user.
    std::string GetDisplayName() const;

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
  std::vector<User> GetUsers() const;

  // Indicates that user just started off the record session.
  void OffTheRecordUserLoggedIn();

  // Indicates that a user with the given email has just logged in.
  // The persistent list will be updated accordingly.
  void UserLoggedIn(const std::string& email);

  // Remove user from persistent list. NOTE: user's data won't be removed.
  void RemoveUser(const std::string& email);

  // Returns the logged-in user.
  const User& logged_in_user() {
    return logged_in_user_;
  }

  // Saves image to file and saves image path in local state preferences.
  void SaveUserImage(const std::string& username,
                     const SkBitmap& image);

  // chromeos::UserImageLoader::Delegate implementation.
  virtual void OnImageLoaded(const std::string& username,
                             const SkBitmap& image);

 private:
  UserManager();
  ~UserManager();

  // Notifies on new user session.
  void NotifyOnLogin();

  // Loads user image from its file.
  scoped_refptr<UserImageLoader> image_loader_;

  // Cache for user images. Stores image for each username.
  typedef base::hash_map<std::string, SkBitmap> UserImages;
  mutable UserImages user_images_;

  // The logged-in user.
  User logged_in_user_;

  DISALLOW_COPY_AND_ASSIGN(UserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
