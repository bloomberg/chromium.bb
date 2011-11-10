// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace chromeos {

// A class representing information about a previously logged in user.
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

  // The email the user used to log in.
  const std::string& email() const { return email_; }

  // Returns the name to display for this user.
  std::string GetDisplayName() const;

  // Tooltip contains user's display name and his email domain to distinguish
  // this user from the other one with the same display name.
  std::string GetNameTooltip() const;

  // Returns true if some users have same display name.
  bool NeedsNameTooltip() const;

  // The image for this user.
  void SetImage(const SkBitmap& image, int image_index);
  const SkBitmap& image() const { return image_; }
  int image_index() const { return image_index_; }

  // OAuth token status for this user.
  OAuthTokenStatus oauth_token_status() const { return oauth_token_status_; }
  void set_oauth_token_status(OAuthTokenStatus status) {
    oauth_token_status_ = status;
  }

 private:
  friend class UserManager;

  // Do not allow anyone else to create new User instances.
  explicit User(const std::string& email);
  ~User();

  std::string email_;
  SkBitmap image_;
  OAuthTokenStatus oauth_token_status_;

  // Either index of a default image for the user, |kExternalImageIndex| or
  // |kProfileImageIndex|.
  int image_index_;

  DISALLOW_COPY_AND_ASSIGN(User);
};

// List of known users.
typedef std::vector<User*> UserList;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_
