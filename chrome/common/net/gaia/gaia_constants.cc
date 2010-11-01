// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Constants definitions

#include "chrome/common/net/gaia/gaia_constants.h"

namespace GaiaConstants {

// Gaia uses this for accounting where login is coming from.
const char kChromeOSSource[] = "chromeos";
const char kChromeSource[] = "ChromiumBrowser";

// Service name for Gaia.  Used to convert to cookie auth.
const char kGaiaService[] = "gaia";
// Service name for Gaia Contacts API. API is used to get user's image.
const char kContactsService[] = "cp";
// Service name for sync.
const char kSyncService[] = "chromiumsync";
// Service name for XMPP Google Talk.
const char kTalkService[] = "talk";
// Service name for remoting.
const char kRemotingService[] = "chromoting";
// Service name for cloud print.
const char kCloudPrintService[] = "cloudprint";
// Service name for device management (cloud-based policy) server.
const char kDeviceManagementService[] = "mobilesync";

}  // namespace GaiaConstants
