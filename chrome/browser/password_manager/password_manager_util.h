// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_H_

#include "base/basictypes.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager_util {

// Attempts to (re-)authenticate the user of the OS account. Returns true if
// the user was successfully authenticated, or if authentication was not
// possible. On platforms where reauthentication is not possible or does not
// make sense, the default implementation always returns true.
bool AuthenticateUser(gfx::NativeWindow window);

}  // namespace password_manager_util

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_H_
