// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user.h"

#include "base/stringprintf.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

// Resource ID of the image to use as stub image.
const int kStubImageResourceID = IDR_PROFILE_PICTURE_LOADING;

// Returns account name portion of an email.
std::string GetUserName(const std::string& email) {
  std::string::size_type i = email.find('@');
  if (i == 0 || i == std::string::npos) {
    return email;
  }
  return email.substr(0, i);
}

}  // namespace

User::User(const std::string& email, bool is_guest)
    : email_(email),
      oauth_token_status_(OAUTH_TOKEN_STATUS_UNKNOWN),
      image_index_(kInvalidImageIndex),
      image_is_stub_(false),
      is_guest_(is_guest) {
  // The email address of a demo user is for internal purposes only,
  // never meant for display.
  if (email != kDemoUser) {
    display_email_ = email;
    is_demo_user_ = false;
  } else {
    is_demo_user_ = true;
  }
  image_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      kDefaultImageResources[0]);
}

User::~User() {}

void User::SetImage(const SkBitmap& image, int image_index) {
  image_ = image;
  image_index_ = image_index;
  image_is_stub_ = false;
}

void User::SetStubImage(int image_index) {
  image_ = *ResourceBundle::GetSharedInstance().
      GetBitmapNamed(kStubImageResourceID);
  image_index_ = image_index;
  image_is_stub_ = true;
}

void User::SetWallpaperThumbnail(const SkBitmap& wallpaper_thumbnail) {
  wallpaper_thumbnail_ = wallpaper_thumbnail;
}

std::string User::GetAccountName() const {
  return GetUserName(email_);
}

std::string User::GetDisplayName() const {
  return GetUserName(display_email_);
}

bool User::NeedsNameTooltip() const {
  return !UserManager::Get()->IsDisplayNameUnique(GetDisplayName());
}

std::string User::GetNameTooltip() const {
  const std::string& user_email = display_email_;
  size_t at_pos = user_email.rfind('@');
  if (at_pos == std::string::npos) {
    NOTREACHED();
    return std::string();
  }
  size_t domain_start = at_pos + 1;
  std::string domain = user_email.substr(domain_start,
                                         user_email.length() - domain_start);
  return base::StringPrintf("%s (%s)",
                            GetDisplayName().c_str(),
                            domain.c_str());
}

}  // namespace chromeos
