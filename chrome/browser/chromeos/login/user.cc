// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user.h"

#include "base/stringprintf.h"
#include "chrome/browser/chromeos/login/default_user_images.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

User::User(const std::string& email)
    : email_(email),
      display_email_(email),
      oauth_token_status_(OAUTH_TOKEN_STATUS_UNKNOWN),
      image_index_(kInvalidImageIndex) {
  image_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      kDefaultImageResources[0]);
}

User::~User() {}

void User::SetImage(const SkBitmap& image, int image_index) {
  image_ = image;
  image_index_ = image_index;
}

std::string User::GetDisplayName() const {
  size_t i = display_email_.find('@');
  if (i == 0 || i == std::string::npos) {
    return display_email_;
  }
  return display_email_.substr(0, i);
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
