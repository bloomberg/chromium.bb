// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/switches.h"

namespace extensions {
namespace switches {

// Path to an app to load at startup.
const char kAppShellAppPath[] = "app-shell-app-path";

// Bounds for the host window to create (i.e. "800x600").
const char kAppShellHostWindowBounds[] = "app-shell-host-window-bounds";

// SSID of the preferred WiFi network.
const char kAppShellPreferredNetwork[] = "app-shell-preferred-network";

// Refresh token for identity API calls for the current user. Used for testing.
const char kAppShellRefreshToken[] = "app-shell-refresh-token";

// User email address of the current user.
const char kAppShellUser[] = "app-shell-user";

}  // namespace switches
}  // namespace extensions
