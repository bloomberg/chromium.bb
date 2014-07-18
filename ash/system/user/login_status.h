// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_USER_LOGIN_STATUS_H_
#define ASH_SYSTEM_USER_LOGIN_STATUS_H_

#include "base/strings/string16.h"

namespace ash {
namespace user {

enum LoginStatus {
  LOGGED_IN_NONE,             // Not logged in
  LOGGED_IN_LOCKED,           // A user has locked the screen
  LOGGED_IN_USER,             // A regular user is logged in
  LOGGED_IN_OWNER,            // The owner of the device is logged in
  LOGGED_IN_GUEST,            // A guest is logged in (i.e. incognito)
  LOGGED_IN_RETAIL_MODE,      // Is in retail mode
  LOGGED_IN_PUBLIC,           // A public account is logged in
  LOGGED_IN_SUPERVISED,       // A supervised user is logged in
  LOGGED_IN_KIOSK_APP         // Is in kiosk app mode
};

base::string16 GetLocalizedSignOutStringForStatus(LoginStatus status,
                                                  bool multiline);

}  // namespace user
}  // namespace ash

#endif  // ASH_SYSTEM_USER_LOGIN_STATUS_H_
