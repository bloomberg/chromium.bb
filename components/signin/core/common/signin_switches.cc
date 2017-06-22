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

#if !BUILDFLAG(ENABLE_MIRROR)
// Command line flag for enabling account consistency. Default mode is disabled.
// Mirror is a legacy mode in which Google accounts are always addded to Chrome,
// and Chrome then adds them to the Google authentication cookies.
// Dice is a new experiment in which Chrome is aware of the accounts in the
// Google authentication cookies.
const char kAccountConsistency[] = "account-consistency";

// Values for the kAccountConsistency flag.
const char kAccountConsistencyMirror[] = "mirror";
const char kAccountConsistencyDice[] = "dice";
#endif

// Enables sending EnableRefreshTokenAnnotationRequest.
extern const char kEnableRefreshTokenAnnotationRequest[] =
    "enable-refresh-token-annotation-request";

// Enables sign-in promo.
const char kEnableSigninPromo[] = "enable-signin-promo";

// Enables multiple account versions of chrome.identity APIs.
const char kExtensionsMultiAccount[] = "extensions-multi-account";

}  // namespace switches
