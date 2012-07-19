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

// The demo user is represented by a domainless username.
const char kDemoUser[] = "demouser";
// Incognito user is represented by an empty string (since some code already
// depends on that and it's hard to figure out what).
const char kGuestUser[] = "";

User::User(const std::string& email)
    : email_(email),
      user_image_(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          kDefaultImageResources[0])),
      oauth_token_status_(OAUTH_TOKEN_STATUS_UNKNOWN),
      image_index_(kInvalidImageIndex),
      image_is_stub_(false) {
  // The email address of a demo user is for internal purposes only,
  // never meant for display.
  if (!is_demo_user())
    display_email_ = email;
}

User::~User() {}

void User::SetImage(const UserImage& user_image, int image_index) {
  user_image_ = user_image;
  image_index_ = image_index;
  image_is_stub_ = false;
}

void User::SetImageURL(const GURL& image_url) {
  user_image_.set_url(image_url);
}

void User::SetStubImage(int image_index) {
  user_image_.SetImage(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      kStubImageResourceID));
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
  // Fallback to the email account name in case display name haven't been set.
  return display_name_.empty() ?
      UTF8ToUTF16(GetAccountName(true)) :
      display_name_;
}

}  // namespace chromeos
