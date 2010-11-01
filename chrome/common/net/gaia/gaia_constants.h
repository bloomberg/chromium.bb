// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
extern const char kContactsService[];
extern const char kTalkService[];
extern const char kSyncService[];
extern const char kRemotingService[];
extern const char kCloudPrintService[];
extern const char kDeviceManagementService[];

}  // namespace GaiaConstants

#endif  // CHROME_COMMON_NET_GAIA_GAIA_CONSTANTS_H_
