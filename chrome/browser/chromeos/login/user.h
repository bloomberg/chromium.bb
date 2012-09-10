// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

// Fake username for the demo user.
extern const char kDemoUser[];

// Username for incognito login.
extern const char kGuestUser[];

extern const int kDefaultImagesCount;

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
    DAILY = 0,
    CUSTOMIZED = 1,
    DEFAULT = 2,
    UNKNOWN = 3,
    ONLINE = 4,
    WALLPAPER_TYPE_COUNT = 5
  };

  // The email the user used to log in.
  const std::string& email() const { return email_; }

  // Returns the human name to display for this user.
  string16 GetDisplayName() const;

  // Returns the account name part of the email. Use the display form of the
  // email if available and use_display_name == true. Otherwise use canonical.
  std::string GetAccountName(bool use_display_email) const;

  // The image for this user.
  const gfx::ImageSkia& image() const { return user_image_.image(); }

  // Whether the user has a default image.
  bool HasDefaultImage() const;

  int image_index() const { return image_index_; }
  bool has_raw_image() const { return user_image_.has_raw_image(); }
  // Returns raw representation of static user image.
  const UserImage::RawImage& raw_image() const {
    return user_image_.raw_image();
  }
  bool has_animated_image() const { return user_image_.has_animated_image(); }
  // Returns raw representation of animated user image.
  const UserImage::RawImage& animated_image() const {
    return user_image_.animated_image();
  }

  // Returns the URL of user image, if there is any. Currently only the profile
  // image has a URL, for other images empty URL is returned.
  GURL image_url() const { return user_image_.url(); }

  // True if user image is a stub (while real image is being loaded from file).
  bool image_is_stub() const { return image_is_stub_; }

  // OAuth token status for this user.
  OAuthTokenStatus oauth_token_status() const { return oauth_token_status_; }

  // The displayed user name.
  string16 display_name() const { return display_name_; }

  // The displayed (non-canonical) user email.
  std::string display_email() const { return display_email_; }

  bool is_demo_user() const { return email_ == kDemoUser; }
  bool is_guest() const { return email_ == kGuestUser; }

 private:
  friend class UserManagerImpl;
  friend class MockUserManager;
  friend class UserManagerTest;

  // Do not allow anyone else to create new User instances.
  explicit User(const std::string& email_guest);
  ~User();

  // Setters are private so only UserManager can call them.
  void SetImage(const UserImage& user_image, int image_index);

  void SetImageURL(const GURL& image_url);

  // Sets a stub image until the next |SetImage| call. |image_index| may be
  // one of |kExternalImageIndex| or |kProfileImageIndex|.
  void SetStubImage(int image_index);

  // Set thumbnail of user custom wallpaper.
  void SetWallpaperThumbnail(const SkBitmap& wallpaper_thumbnail);

  void set_oauth_token_status(OAuthTokenStatus status) {
    oauth_token_status_ = status;
  }

  void set_display_name(const string16& display_name) {
    display_name_ = display_name;
  }

  void set_display_email(const std::string& display_email) {
    display_email_ = display_email;
  }

  std::string email_;
  string16 display_name_;
  // The displayed user email, defaults to |email_|.
  std::string display_email_;
  UserImage user_image_;
  OAuthTokenStatus oauth_token_status_;

  // Either index of a default image for the user, |kExternalImageIndex| or
  // |kProfileImageIndex|.
  int image_index_;

  // True if current user image is a stub set by a |SetStubImage| call.
  bool image_is_stub_;

  DISALLOW_COPY_AND_ASSIGN(User);
};

// List of known users.
typedef std::vector<User*> UserList;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_
