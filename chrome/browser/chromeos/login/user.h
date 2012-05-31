// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"

// The demo user is represented by a domainless username.
const char kDemoUser[] = "demouser";

namespace chromeos {

// A class representing information about a previously logged in user.
// Each user has a canonical email (username), returned by |email()| and
// may have a different displayed email (in the raw form as entered by user),
// returned by |displayed_email()|.
// Displayed emails are for use in UI only, anywhere else users must be referred
// to by |email()|.
class User {
 public:
  // User OAuth token status according to the last check.
  typedef enum {
    OAUTH_TOKEN_STATUS_UNKNOWN = 0,
    OAUTH_TOKEN_STATUS_INVALID = 1,
    OAUTH_TOKEN_STATUS_VALID   = 2,
  } OAuthTokenStatus;

  // Returned as |image_index| when user-selected file or photo is used as
  // user image.
  static const int kExternalImageIndex = -1;
  // Returned as |image_index| when user profile image is used as user image.
  static const int kProfileImageIndex = -2;
  static const int kInvalidImageIndex = -3;

  enum WallpaperType {
    RANDOM = 0,
    CUSTOMIZED = 1,
    DEFAULT = 2,
    UNKNOWN = 3
  };

  // The email the user used to log in.
  const std::string& email() const { return email_; }

  // Returns the name to display for this user.
  std::string GetDisplayName() const;
  // Returns the account name part of the email.
  std::string GetAccountName() const;

  // Tooltip contains user's display name and his email domain to distinguish
  // this user from the other one with the same display name.
  std::string GetNameTooltip() const;

  // Returns true if some users have same display name.
  bool NeedsNameTooltip() const;

  // The image for this user.
  const SkBitmap& image() const { return image_; }
  int image_index() const { return image_index_; }

  // The thumbnail of user custom wallpaper.
  const SkBitmap& wallpaper_thumbnail() const { return wallpaper_thumbnail_; }

  // True if user image is a stub (while real image is being loaded from file).
  bool image_is_stub() const { return image_is_stub_; }

  // OAuth token status for this user.
  OAuthTokenStatus oauth_token_status() const { return oauth_token_status_; }

  // The displayed (non-canonical) user email.
  std::string display_email() const { return display_email_; }

  bool is_demo_user() const { return is_demo_user_; }
  bool is_guest() const { return is_guest_; }

 private:
  friend class UserManagerImpl;
  friend class MockUserManager;
  friend class UserManagerTest;

  // Do not allow anyone else to create new User instances.
  User(const std::string& email, bool is_guest);
  ~User();

  // Setters are private so only UserManager can call them.
  void SetImage(const SkBitmap& image, int image_index);
  // Sets a stub image until the next |SetImage| call. |image_index| may be
  // one of |kExternalImageIndex| or |kProfileImageIndex|.
  void SetStubImage(int image_index);

  // Set thumbnail of user custom wallpaper.
  void SetWallpaperThumbnail(const SkBitmap& wallpaper_thumbnail);

  void set_oauth_token_status(OAuthTokenStatus status) {
    oauth_token_status_ = status;
  }

  void set_display_email(const std::string& display_email) {
    display_email_ = display_email;
  }

  std::string email_;
  // The displayed user email, defaults to |email_|.
  std::string display_email_;
  SkBitmap image_;
  OAuthTokenStatus oauth_token_status_;
  SkBitmap wallpaper_thumbnail_;

  // Either index of a default image for the user, |kExternalImageIndex| or
  // |kProfileImageIndex|.
  int image_index_;

  // True if current user image is a stub set by a |SetStubImage| call.
  bool image_is_stub_;

  // Is this a guest account?
  bool is_guest_;

  // Is this a demo user account?
  bool is_demo_user_;

  DISALLOW_COPY_AND_ASSIGN(User);
};

// List of known users.
typedef std::vector<User*> UserList;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_
