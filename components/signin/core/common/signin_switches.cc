// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/signin_switches.h"

namespace switches {

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

// Disables sign-in promo.
const char kDisableSigninPromo[] = "disable-signin-promo";

// Disables sending signin scoped device id to LSO with refresh token request.
const char kDisableSigninScopedDeviceId[] = "disable-signin-scoped-device-id";

#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Enables consistent identity features.
const char kEnableAccountConsistency[] = "enable-account-consistency";
#endif

// Enables sending EnableRefreshTokenAnnotationRequest.
extern const char kEnableRefreshTokenAnnotationRequest[] =
    "enable-refresh-token-annotation-request";

// Enables sign-in promo.
const char kEnableSigninPromo[] = "enable-signin-promo";

// Enables multiple account versions of chrome.identity APIs.
const char kExtensionsMultiAccount[] = "extensions-multi-account";

}  // namespace switches
