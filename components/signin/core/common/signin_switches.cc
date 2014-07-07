// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/signin_switches.h"

namespace switches {

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

// Disables consistent identity features.
const char kDisableAccountConsistency[] = "disable-account-consistency";

// Disables new profile management system, including new profile chooser UI.
const char kDisableNewProfileManagement[] = "disable-new-profile-management";

// Enables consistent identity features.
const char kEnableAccountConsistency[] = "enable-account-consistency";

// Enables new profile management system, including new profile chooser UI.
const char kEnableNewProfileManagement[] = "new-profile-management";

// Enable sending signin scopde device id to LSO with refresh token request.
const char kEnableSigninScopedDeviceId[] = "enable-signin-scoped-device-id";

// Enables the pure web-based flow for sign in on first run/NTP/wrench menu/
// settings page.
const char kEnableWebBasedSignin[] = "enable-web-based-signin";

// Enables multiple account versions of chrome.identity APIs.
const char kExtensionsMultiAccount[] = "extensions-multi-account";

// Allows displaying the list of existing profiles in the avatar bubble for
// fast switching between profiles.
const char kFastUserSwitching[] = "fast-user-switching";

// Enables using GAIA information to populate profile name and icon.
const char kGoogleProfileInfo[] = "google-profile-info";

// Use new avatar menu. When combined with new-profile-management, it simply
// shows the new profile management avatar menu. Otherwise it shows a redesigned
// avatar menu with the same functionality as the old one, plus a tutorial card
// at the top prompting the user to try out the new profile management UI.
const char kNewAvatarMenu[] = "new-avatar-menu";

}  // namespace switches
