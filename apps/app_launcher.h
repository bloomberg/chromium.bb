// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_LAUNCHER_H_
#define APPS_APP_LAUNCHER_H_

#include "base/callback_forward.h"

namespace apps {

// Returns whether the app launcher has been enabled.
bool IsAppLauncherEnabled();

// Returns whether the app launcher promo should be shown.
bool ShouldShowAppLauncherPromo();

}  // namespace apps

#endif  // APPS_APP_LAUNCHER_H_
