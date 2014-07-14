// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/empty_user_info.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

namespace user_manager {

EmptyUserInfo::EmptyUserInfo() {
}

EmptyUserInfo::~EmptyUserInfo() {
}

base::string16 EmptyUserInfo::GetDisplayName() const {
  NOTIMPLEMENTED();
  return base::UTF8ToUTF16(std::string());
}

base::string16 EmptyUserInfo::GetGivenName() const {
  NOTIMPLEMENTED();
  return base::UTF8ToUTF16(std::string());
}

std::string EmptyUserInfo::GetEmail() const {
  NOTIMPLEMENTED();
  return std::string();
}

std::string EmptyUserInfo::GetUserID() const {
  NOTIMPLEMENTED();
  return std::string();
}

const gfx::ImageSkia& EmptyUserInfo::GetImage() const {
  NOTIMPLEMENTED();
  // To make the compiler happy.
  return null_image_;
}

}  // namespace user_manager
