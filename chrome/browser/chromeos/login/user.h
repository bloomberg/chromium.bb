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

// The guest user has a magic, empty e-mail address.
extern const char kGuestUserEMail[];

// The retail mode user has a magic, domainless e-mail address.
extern const char kRetailModeUserEMail[];

extern const int kDefaultImagesCount;

// User context data that is being exchanged between part of ChromeOS
// authentication mechanism. Includes credentials:
// |username|, |password|, |auth_code| and |username_hash| which is returned
// back once user homedir is mounted. |username_hash| is used to identify
// user homedir mount point.
struct UserContext {
  UserContext();
  UserContext(const std::string& username,
              const std::string& password,
              const std::string& auth_code);
  UserContext(const std::string& username,
              const std::string& password,
              const std::string& auth_code,
              const std::string& username_hash);
  virtual ~UserContext();
  bool operator==(const UserContext& context) const;
  std::string username;
  std::string password;
  std::string auth_code;
  std::string username_hash;
};

// A class representing information about a previously logged in user.
// Each user has a canonical email (username), returned by |email()| and
// may have a different displayed email (in the raw form as entered by user),
// returned by |displayed_email()|.
// Displayed emails are for use in UI only, anywhere else users must be referred
// to by |email()|.
class User {
 public:
  // The user type.
  typedef enum {
    // Regular user, has a user name and password.
    USER_TYPE_REGULAR = 0,
    // Guest user, logs in without authentication.
    USER_TYPE_GUEST,
    // Retail mode user, logs in without authentication. This is a special user
    // type used in retail mode only.
    USER_TYPE_RETAIL_MODE,
    // Public account user, logs in without authentication. Available only if
    // enabled through policy.
    USER_TYPE_PUBLIC_ACCOUNT,
    // Locally managed user, logs in only with local authentication.
    USER_TYPE_LOCALLY_MANAGED,
    // Kiosk app robot, logs in without authentication.
    USER_TYPE_KIOSK_APP
  } UserType;

  // User OAuth token status according to the last check.
  // Please note that enum values 1 and 2 were used for OAuth1 status and are
  // deprecated now.
  typedef enum {
     OAUTH_TOKEN_STATUS_UNKNOWN  = 0,
     OAUTH2_TOKEN_STATUS_INVALID = 3,
     OAUTH2_TOKEN_STATUS_VALID   = 4,
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

  // Returns the user type.
  virtual UserType GetType() const = 0;

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

  // Whether |raw_image| contains data in format that is considered safe to
  // decode in sensitive environment (on Login screen).
  bool image_is_safe_format() const { return user_image_.is_safe_format(); }

  // Returns the URL of user image, if there is any. Currently only the profile
  // image has a URL, for other images empty URL is returned.
  GURL image_url() const { return user_image_.url(); }

  // True if user image is a stub (while real image is being loaded from file).
  bool image_is_stub() const { return image_is_stub_; }

  // True if image is being loaded from file.
  bool image_is_loading() const { return image_is_loading_; }

  // OAuth token status for this user.
  OAuthTokenStatus oauth_token_status() const { return oauth_token_status_; }

  // The displayed user name.
  string16 display_name() const { return display_name_; }

  // The displayed (non-canonical) user email.
  virtual std::string display_email() const;

  // True if the user's session can be locked (i.e. the user has a password with
  // which to unlock the session).
  virtual bool can_lock() const;

  virtual std::string username_hash() const;

 protected:
  friend class UserManagerImpl;
  friend class UserImageManagerImpl;
  // For testing:
  friend class MockUserManager;

  // Do not allow anyone else to create new User instances.
  static User* CreateRegularUser(const std::string& email);
  static User* CreateGuestUser();
  static User* CreateKioskAppUser(const std::string& kiosk_app_username);
  static User* CreateLocallyManagedUser(const std::string& username);
  static User* CreateRetailModeUser();
  static User* CreatePublicAccountUser(const std::string& email);

  explicit User(const std::string& email);
  virtual ~User();

  // Setters are private so only UserManager can call them.
  void SetImage(const UserImage& user_image, int image_index);

  void SetImageURL(const GURL& image_url);

  // Sets a stub image until the next |SetImage| call. |image_index| may be
  // one of |kExternalImageIndex| or |kProfileImageIndex|.
  // If |is_loading| is |true|, that means user image is being loaded from file.
  void SetStubImage(int image_index, bool is_loading);

  void set_oauth_token_status(OAuthTokenStatus status) {
    oauth_token_status_ = status;
  }

  void set_display_name(const string16& display_name) {
    display_name_ = display_name;
  }

  void set_display_email(const std::string& display_email) {
    display_email_ = display_email;
  }

  const UserImage& user_image() const { return user_image_; }

  void set_username_hash(const std::string& username_hash) {
    username_hash_ = username_hash;
  }

 private:
  std::string email_;
  string16 display_name_;
  // The displayed user email, defaults to |email_|.
  std::string display_email_;
  UserImage user_image_;
  OAuthTokenStatus oauth_token_status_;

  // Used to identify homedir mount point.
  std::string username_hash_;

  // Either index of a default image for the user, |kExternalImageIndex| or
  // |kProfileImageIndex|.
  int image_index_;

  // True if current user image is a stub set by a |SetStubImage| call.
  bool image_is_stub_;

  // True if current user image is being loaded from file.
  bool image_is_loading_;

  DISALLOW_COPY_AND_ASSIGN(User);
};

// List of known users.
typedef std::vector<User*> UserList;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_H_
