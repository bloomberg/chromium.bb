// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_USER_INFO_H_
#define ASH_SESSION_USER_INFO_H_

#include <string>

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace gfx {
class ImageSkia;
}

namespace ash {

// A class that represents user related info.
class ASH_EXPORT UserInfo {
 public:
  virtual ~UserInfo() {}

  // Gets the display name for the user.
  virtual base::string16 GetDisplayName() const = 0;

  // Gets the given name of the user.
  virtual base::string16 GetGivenName() const = 0;

  // Gets the display email address for the user.
  // The display email address might contains some periods in the email name
  // as well as capitalized letters. For example: "Foo.Bar@mock.com".
  virtual std::string GetEmail() const = 0;

  // Gets the user id (sanitized email address) for the user.
  // The function would return something like "foobar@mock.com".
  virtual std::string GetUserID() const = 0;

  // Gets the avatar image for the user.
  virtual const gfx::ImageSkia& GetImage() const = 0;
};

}  // namespace ash

#endif  // ASH_SESSION_USER_INFO_H_
