// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants used by IssueAuthToken and ClientLogin

#ifndef GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_
#define GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_

namespace GaiaConstants {

// Gaia sources for accounting
extern const char kChromeOSSource[];
extern const char kChromeSource[];

// Gaia services for requesting
extern const char kGaiaService[];  // uber token
extern const char kPicasaService[];
extern const char kSyncService[];
extern const char kRemotingService[];
extern const char kCloudPrintService[];
extern const char kDeviceManagementServiceOAuth[];
extern const char kCWSService[];
extern const char kCWSNotificationScope[];
extern const char kLSOService[];

// Used with uber auth tokens when needed.
extern const char kGaiaSid[];
extern const char kGaiaLsid[];
extern const char kGaiaOAuthToken[];
extern const char kGaiaOAuthSecret[];
extern const char kGaiaOAuthDuration[];
extern const char kGaiaOAuth2LoginRefreshToken[];

// Used to construct a channel ID for push messaging.
extern const char kObfuscatedGaiaId[];

// Used to build ClientOAuth requests.  These are the names of keys used in
// the json dictionaries that are sent in the protocol.
extern const char kClientOAuthEmailKey[];
extern const char kClientOAuthPasswordKey[];
extern const char kClientOAuthScopesKey[];
extern const char kClientOAuthOAuth2ClientIdKey[];
extern const char kClientOAuthFriendlyDeviceNameKey[];
extern const char kClientOAuthAcceptsChallengesKey[];
extern const char kClientOAuthLocaleKey[];
extern const char kClientOAuthFallbackNameKey[];
extern const char kClientOAuthNameKey[];
extern const char kClientOAuthChallengeTokenKey[];
extern const char kClientOAuthchallengeReplyKey[];

}  // namespace GaiaConstants

#endif  // GOOGLE_APIS_GAIA_GAIA_CONSTANTS_H_
