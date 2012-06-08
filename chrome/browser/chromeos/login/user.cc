// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user.h"

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
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
      image_(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          kDefaultImageResources[0])),
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
}

User::~User() {}

void User::SetImage(const gfx::ImageSkia& image, int image_index) {
  image_ = image;
  image_index_ = image_index;
  image_is_stub_ = false;
}

void User::SetStubImage(int image_index) {
  image_ = *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      kStubImageResourceID);
  image_index_ = image_index;
  image_is_stub_ = true;
}

void User::SetWallpaperThumbnail(const SkBitmap& wallpaper_thumbnail) {
  wallpaper_thumbnail_ = wallpaper_thumbnail;
}

std::string User::GetAccountName(bool use_display_email) const {
  if (use_display_email && !display_email_.empty())
    return GetUserName(display_email_);
  else
    return GetUserName(email_);
}

string16 User::GetDisplayName() const {
  return display_name_;
}

}  // namespace chromeos
