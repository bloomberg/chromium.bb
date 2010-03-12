// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"

// This class provides a mechanism for discovering users who have logged
// into this chromium os device before and updating that list.
namespace chromeos {
class UserManager {

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
    friend class UserManager;

    std::string email_;
    SkBitmap image_;
  };

  // Gets a shared instance of a UserManager. Not thread-safe...should
  // only be called from the main UI thread.
  static UserManager* Get();

  // Returns a list of the users who have logged into this device previously.
  // It is sorted in order of recency, with most recent at the beginning.
  std::vector<User> GetUsers() const;

  // Indicates that a user with the given email has just logged in.
  // The persistent list will be updated accordingly.
  void UserLoggedIn(const std::string& email);

 private:
  UserManager() {}
  ~UserManager() {}

  DISALLOW_COPY_AND_ASSIGN(UserManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_MANAGER_H_
