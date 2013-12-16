// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_H_

#include "base/basictypes.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager_util {

enum OsPasswordStatus {
  PASSWORD_STATUS_UNKNOWN = 0,
  PASSWORD_STATUS_UNSUPPORTED,
  PASSWORD_STATUS_BLANK,
  PASSWORD_STATUS_NONBLANK,
  PASSWORD_STATUS_WIN_DOMAIN,
  // NOTE: Add new status types only immediately above this line. Also,
  // make sure the enum list in tools/histogram/histograms.xml is
  // updated with any change in here.
  MAX_PASSWORD_STATUS
};

// Attempts to (re-)authenticate the user of the OS account. Returns true if
// the user was successfully authenticated, or if authentication was not
// possible. On platforms where reauthentication is not possible or does not
// make sense, the default implementation always returns true.
bool AuthenticateUser(gfx::NativeWindow window);

// Query the system to determine whether the current logged on user has a
// password set on their OS account.  Returns one of the OsPasswordStatus
// enum values.
OsPasswordStatus GetOsPasswordStatus();

}  // namespace password_manager_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_H_
