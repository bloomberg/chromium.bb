// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_EMPTY_USER_INFO_H_
#define COMPONENTS_USER_MANAGER_EMPTY_USER_INFO_H_

#include <string>

#include "base/strings/string16.h"
#include "components/user_manager/user_info.h"
#include "components/user_manager/user_manager_export.h"
#include "ui/gfx/image/image_skia.h"

namespace user_manager {

// Trivial implementation of UserInfo interface which triggers
// NOTIMPLEMENTED() for each method.
class USER_MANAGER_EXPORT EmptyUserInfo : public UserInfo {
 public:
  EmptyUserInfo();
  virtual ~EmptyUserInfo();

  // UserInfo:
  virtual base::string16 GetDisplayName() const OVERRIDE;
  virtual base::string16 GetGivenName() const OVERRIDE;
  virtual std::string GetEmail() const OVERRIDE;
  virtual std::string GetUserID() const OVERRIDE;
  virtual const gfx::ImageSkia& GetImage() const OVERRIDE;

 private:
  const gfx::ImageSkia null_image_;

  DISALLOW_COPY_AND_ASSIGN(EmptyUserInfo);
};

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_EMPTY_USER_INFO_H_
