// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_constants.h"

namespace local_discovery {

const char kPrivetKeyError[] = "error";
const char kPrivetInfoKeyToken[] = "x-privet-token";
const char kPrivetKeyDeviceID[] = "device_id";
const char kPrivetKeyClaimURL[] = "claim_url";
const char kPrivetKeyTimeout[] = "timeout";

const char kPrivetErrorDeviceBusy[] = "device_busy";
const char kPrivetErrorPendingUserAction[] = "pending_user_action";
const char kPrivetErrorInvalidXPrivetToken[] = "invalid_x_privet_token";

const char kPrivetActionStart[] = "start";
const char kPrivetActionGetClaimToken[] = "getClaimToken";
const char kPrivetActionComplete[] = "complete";

}  // namespace local_discovery
