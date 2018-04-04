// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_
#define CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_

#include "base/callback_forward.h"

namespace chromeos {

enum class AppsNavigationAction {
  // The current navigation should be cancelled.
  CANCEL,

  // The current navigation should resume.
  RESUME,
};

// Callback to allow app-platform-specific code to asynchronously signal what
// action should be taken for the current navigation.
using AppsNavigationCallback =
    base::OnceCallback<void(AppsNavigationAction action)>;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APPS_INTENT_HELPER_APPS_NAVIGATION_TYPES_H_
