// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants used by IssueAuthToken and ClientLogin

#ifndef CHROME_COMMON_NET_GAIA_GAIA_CONSTANTS_H_
#define CHROME_COMMON_NET_GAIA_GAIA_CONSTANTS_H_

namespace GaiaConstants {

// Gaia sources for accounting
extern const char kChromeOSSource[];
extern const char kChromeSource[];

// Gaia services for requesting
extern const char kGaiaService[];  // uber token
extern const char kPicasaService[];
extern const char kTalkService[];
extern const char kSyncService[];
extern const char kSyncServiceOAuth[];
extern const char kRemotingService[];
extern const char kCloudPrintService[];
extern const char kDeviceManagementService[];
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
extern const char kGaiaOAuth2LoginAccessToken[];

}  // namespace GaiaConstants

#endif  // CHROME_COMMON_NET_GAIA_GAIA_CONSTANTS_H_
