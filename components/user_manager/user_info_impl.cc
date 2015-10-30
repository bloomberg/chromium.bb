// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/user_info_impl.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

namespace user_manager {

UserInfoImpl::UserInfoImpl() {
}

UserInfoImpl::~UserInfoImpl() {
}

base::string16 UserInfoImpl::GetDisplayName() const {
  return base::UTF8ToUTF16("stub-user");
}

base::string16 UserInfoImpl::GetGivenName() const {
  return base::UTF8ToUTF16("Stub");
}

std::string UserInfoImpl::GetEmail() const {
  return "stub-user@domain.com";
}

std::string UserInfoImpl::GetUserID() const {
  return GetEmail();
}

const gfx::ImageSkia& UserInfoImpl::GetImage() const {
  return user_image_;
}

}  // namespace user_manager
