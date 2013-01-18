// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_LAUNCHER_H_
#define CHROME_BROWSER_EXTENSIONS_APP_LAUNCHER_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"

namespace extensions {

// Called on the UI thread after determining if the launcher is enabled. A
// boolean flag is passed, which is true if the app launcher is enabled.
typedef base::Callback<void(bool)> OnAppLauncherEnabledCompleted;

// Determine whether the app launcher is enabled or not. This may involve a trip
// to a blocking thread. |completion_callback| is called when an answer is
// ready. This needs to be called on the UI thread.
void GetIsAppLauncherEnabled(
    const OnAppLauncherEnabledCompleted& completion_callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_APP_LAUNCHER_H_
