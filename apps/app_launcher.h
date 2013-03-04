// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_LAUNCHER_H_
#define APPS_APP_LAUNCHER_H_

#include "base/callback_forward.h"

namespace apps {

// Called on the UI thread after determining if the launcher is enabled. A
// boolean flag is passed, which is true if the app launcher is enabled.
typedef base::Callback<void(bool)> OnAppLauncherEnabledCompleted;

// A synchronous check to determine if the app launcher is enabled. If the
// registry needs to be determined to find an accurate answer, this function
// will NOT do so; instead if will default to false (the app launcher is not
// enabled).
// This function does not use the cached preference of whether the launcher
// was enabled or not.
bool MaybeIsAppLauncherEnabled();

// Determine whether the app launcher is enabled or not. This may involve a trip
// to a blocking thread. |completion_callback| is called when an answer is
// ready. This needs to be called on the UI thread.
void GetIsAppLauncherEnabled(
    const OnAppLauncherEnabledCompleted& completion_callback);

// Returns whether the app launcher was enabled the last time it was checked.
bool WasAppLauncherEnabled();

}  // namespace apps

#endif  // APPS_APP_LAUNCHER_H_
