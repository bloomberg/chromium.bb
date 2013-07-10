// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MODE_APP_MODE_UTILS_H_
#define CHROME_BROWSER_APP_MODE_APP_MODE_UTILS_H_

namespace chrome {

// Returns true if the given browser command is allowed in app mode.
bool IsCommandAllowedInAppMode(int command_id);

// Return true if browser process is run in kiosk or forced app mode.
bool IsRunningInAppMode();

// Return true if browser process is run in forced app mode.
bool IsRunningInForcedAppMode();

}  // namespace chrome

#endif  // CHROME_BROWSER_APP_MODE_APP_MODE_UTILS_H_
