// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/user.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chromeos/login/user_names.h"
#include "components/user_manager/user_image/default_user_images.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

// Returns account name portion of an email.
std::string GetUserName(const std::string& email) {
  std::string::size_type i = email.find('@');
  if (i == 0 || i == std::string::npos) {
    return email;
  }
  return email.substr(0, i);
}

}  // namespace

const int User::kExternalImageIndex;
const int User::kProfileImageIndex;
const int User::kInvalidImageIndex;

class RegularUser : public User {
 public:
  explicit RegularUser(const std::string& email);
  virtual ~RegularUser();

  // Overridden from User:
  virtual user_manager::UserType GetType() const OVERRIDE;
  virtual bool CanSyncImage() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegularUser);
};

class GuestUser : public User {
 public:
  GuestUser();
  virtual ~GuestUser();

  // Overridden from User:
  virtual user_manager::UserType GetType() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestUser);
};

class KioskAppUser : public User {
 public:
  explicit KioskAppUser(const std::string& app_id);
  virtual ~KioskAppUser();

  // Overridden from User:
  virtual user_manager::UserType GetType() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(KioskAppUser);
};

class SupervisedUser : public User {
 public:
  explicit SupervisedUser(const std::string& username);
  virtual ~SupervisedUser();

  // Overridden from User:
  virtual user_manager::UserType GetType() const OVERRIDE;
  virtual std::string display_email() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisedUser);
};

class RetailModeUser : public User {
 public:
  RetailModeUser();
  virtual ~RetailModeUser();

  // Overridden from User:
  virtual user_manager::UserType GetType() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(RetailModeUser);
};

class PublicAccountUser : public User {
 public:
  explicit PublicAccountUser(const std::string& email);
  virtual ~PublicAccountUser();

  // Overridden from User:
  virtual user_manager::UserType GetType() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PublicAccountUser);
};

std::string User::GetEmail() const {
  return display_email();
}

base::string16 User::GetDisplayName() const {
  // Fallback to the email account name in case display name haven't been set.
  return display_name_.empty() ?
      base::UTF8ToUTF16(GetAccountName(true)) :
      display_name_;
}

base::string16 User::GetGivenName() const {
  return given_name_;
}

const gfx::ImageSkia& User::GetImage() const {
  return user_image_.image();
}

std::string User::GetUserID() const {
  return gaia::CanonicalizeEmail(gaia::SanitizeEmail(email()));
}

std::string User::GetAccountName(bool use_display_email) const {
  if (use_display_email && !display_email_.empty())
    return GetUserName(display_email_);
  else
    return GetUserName(email_);
}

bool User::HasDefaultImage() const {
  return image_index_ >= 0 && image_index_ < user_manager::kDefaultImagesCount;
}

bool User::CanSyncImage() const {
  return false;
}

std::string User::display_email() const {
  return display_email_;
}

bool User::can_lock() const {
  return can_lock_;
}

std::string User::username_hash() const {
  return username_hash_;
}

bool User::is_logged_in() const {
  return is_logged_in_;
}

bool User::is_active() const {
  return is_active_;
}

User* User::CreateRegularUser(const std::string& email) {
  return new RegularUser(email);
}

User* User::CreateGuestUser() {
  return new GuestUser;
}

User* User::CreateKioskAppUser(const std::string& kiosk_app_username) {
  return new KioskAppUser(kiosk_app_username);
}

User* User::CreateSupervisedUser(const std::string& username) {
  return new SupervisedUser(username);
}

User* User::CreateRetailModeUser() {
  return new RetailModeUser;
}

User* User::CreatePublicAccountUser(const std::string& email) {
  return new PublicAccountUser(email);
}

User::User(const std::string& email)
    : email_(email),
      oauth_token_status_(OAUTH_TOKEN_STATUS_UNKNOWN),
      force_online_signin_(false),
      image_index_(kInvalidImageIndex),
      image_is_stub_(false),
      image_is_loading_(false),
      can_lock_(false),
      is_logged_in_(false),
      is_active_(false),
      profile_is_created_(false) {
}

User::~User() {}

void User::SetAccountLocale(const std::string& resolved_account_locale) {
  account_locale_.reset(new std::string(resolved_account_locale));
}

void User::SetImage(const user_manager::UserImage& user_image,
                    int image_index) {
  user_image_ = user_image;
  image_index_ = image_index;
  image_is_stub_ = false;
  image_is_loading_ = false;
  DCHECK(HasDefaultImage() || user_image.has_raw_image());
}

void User::SetImageURL(const GURL& image_url) {
  user_image_.set_url(image_url);
}

void User::SetStubImage(int image_index, bool is_loading) {
  user_image_ = user_manager::UserImage(
      *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_PROFILE_PICTURE_LOADING));
  image_index_ = image_index;
  image_is_stub_ = true;
  image_is_loading_ = is_loading;
}

RegularUser::RegularUser(const std::string& email) : User(email) {
  set_can_lock(true);
  set_display_email(email);
}

RegularUser::~RegularUser() {}

user_manager::UserType RegularUser::GetType() const {
  return user_manager::USER_TYPE_REGULAR;
}

bool RegularUser::CanSyncImage() const {
  return true;
}

GuestUser::GuestUser() : User(chromeos::login::kGuestUserName) {
  set_display_email(std::string());
}

GuestUser::~GuestUser() {}

user_manager::UserType GuestUser::GetType() const {
  return user_manager::USER_TYPE_GUEST;
}

KioskAppUser::KioskAppUser(const std::string& kiosk_app_username)
    : User(kiosk_app_username) {
  set_display_email(kiosk_app_username);
}

KioskAppUser::~KioskAppUser() {}

user_manager::UserType KioskAppUser::GetType() const {
  return user_manager::USER_TYPE_KIOSK_APP;
}

SupervisedUser::SupervisedUser(const std::string& username)
    : User(username) {
  set_can_lock(true);
}

SupervisedUser::~SupervisedUser() {}

user_manager::UserType SupervisedUser::GetType() const {
  return user_manager::USER_TYPE_SUPERVISED;
}

std::string SupervisedUser::display_email() const {
  return base::UTF16ToUTF8(display_name());
}

RetailModeUser::RetailModeUser() : User(chromeos::login::kRetailModeUserName) {
  set_display_email(std::string());
}

RetailModeUser::~RetailModeUser() {}

user_manager::UserType RetailModeUser::GetType() const {
  return user_manager::USER_TYPE_RETAIL_MODE;
}

PublicAccountUser::PublicAccountUser(const std::string& email) : User(email) {
}

PublicAccountUser::~PublicAccountUser() {}

user_manager::UserType PublicAccountUser::GetType() const {
  return user_manager::USER_TYPE_PUBLIC_ACCOUNT;
}

bool User::has_gaia_account() const {
  COMPILE_ASSERT(user_manager::NUM_USER_TYPES == 6, num_user_types_unexpected);
  switch (GetType()) {
    case user_manager::USER_TYPE_REGULAR:
      return true;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_RETAIL_MODE:
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    case user_manager::USER_TYPE_SUPERVISED:
    case user_manager::USER_TYPE_KIOSK_APP:
      return false;
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace chromeos
