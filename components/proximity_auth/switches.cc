// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/switches.h"

namespace proximity_auth {
namespace switches {

// Overrides the default URL for Google APIs (https://www.googleapis.com) used
// by CryptAuth.
const char kCryptAuthHTTPHost[] = "cryptauth-http-host";

// Disable Easy sign-in.
const char kDisableEasySignin[] = "disable-easy-signin";

// Disable Easy unlock.
const char kDisableEasyUnlock[] = "disable-easy-unlock";

// TODO(xiyuan): Remove obsolete enable flags since feature is on by default.
//    Also consolidate the two flags into one after http://crbug.com/439638.

// Enable Easy sign-in.
const char kEnableEasySignin[] = "enable-easy-signin";

// Enable Easy unlock.
const char kEnableEasyUnlock[] = "enable-easy-unlock";

// Enables close proximity detection. This allows the user to set a setting to
// require very close proximity between the remote device and the local device
// in order to unlock the local device, which trades off convenience for
// security.
const char kEnableProximityDetection[] =
    "enable-proximity-auth-proximity-detection";

}  // namespace switches
}  // namespace proximity_auth
