// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants definitions

#include "google_apis/gaia/gaia_constants.h"

namespace GaiaConstants {

// Gaia uses this for accounting where login is coming from.
const char kChromeOSSource[] = "chromeos";
const char kChromeSource[] = "ChromiumBrowser";

// Service name for Gaia.  Used to convert to cookie auth.
const char kGaiaService[] = "gaia";
// Service name for Picasa API. API is used to get user's image.
const char kPicasaService[] = "lh2";

// Service/scope names for sync.
const char kSyncService[] = "chromiumsync";

// Service name for remoting.
const char kRemotingService[] = "chromoting";
// Service name for cloud print.
const char kCloudPrintService[] = "cloudprint";

// Service/scope names for device management (cloud-based policy) server.
const char kDeviceManagementServiceOAuth[] =
    "https://www.googleapis.com/auth/chromeosdevicemanagement";

// OAuth scopes for chrome web store.
const char kCWSNotificationScope[] =
    "https://www.googleapis.com/auth/chromewebstore.notification";

// Service for LSO endpoint of Google that exposes OAuth APIs.
const char kLSOService[] = "lso";

// Used to mint uber auth tokens when needed.
const char kGaiaSid[] = "sid";
const char kGaiaLsid[] = "lsid";
const char kGaiaOAuthToken[] = "oauthToken";
const char kGaiaOAuthSecret[] = "oauthSecret";
const char kGaiaOAuthDuration[] = "3600";
const char kGaiaOAuth2LoginRefreshToken[] = "oauth2LoginRefreshToken";

// Used to construct a channel ID for push messaging.
const char kObfuscatedGaiaId[] = "obfuscatedGaiaId";

// Used to build ClientOAuth requests.  These are the names of keys used when
// building base::DictionaryValue that represent the json data that makes up
// the ClientOAuth endpoint protocol.  The comment above each constant explains
// what value is associated with that key.

// Canonical email and password of the account to sign in.
const char kClientOAuthEmailKey[] = "email";
const char kClientOAuthPasswordKey[] = "password";

// Scopes required for the returned oauth2 token.  For GaiaAuthFetcher, the
// value is the OAuthLogin scope.
const char kClientOAuthScopesKey[] = "scopes";

// Chrome's client id from the API console.
const char kClientOAuthOAuth2ClientIdKey[] = "oauth2_client_id";

// A friendly name to describe this instance of chrome to the user.
const char kClientOAuthFriendlyDeviceNameKey[] = "friendly_device_name";

// A list of challenge types that chrome accepts.  At a minimum this must
// include Captcha.  To support OTPs should also include TwoFactor.
const char kClientOAuthAcceptsChallengesKey[] = "accepts_challenges";

// The locale of the browser, so that ClientOAuth can return localized error
// messages.
const char kClientOAuthLocaleKey[] = "locale";

// The name of the web-based fallback method to use if ClientOAuth decides it
// cannot continue otherwise.  Note that this name has a dot because its in
// sub dictionary.
const char kClientOAuthFallbackNameKey[] = "fallback.name";

// The following three key names are used with ClientOAuth challenge responses.

// The type of response.  Must match the name given in the response to the
// original ClientOAuth request and is a subset of the challenge types listed
// in kClientOAuthAcceptsChallengesKey from that original request.
const char kClientOAuthNameKey[] = "name";

// The challenge token received in the original ClientOAuth request.
const char kClientOAuthChallengeTokenKey[] = "challenge_token";

// The dictionary that contains the challenge response.
const char kClientOAuthchallengeReplyKey[] = "challenge_reply";

}  // namespace GaiaConstants
