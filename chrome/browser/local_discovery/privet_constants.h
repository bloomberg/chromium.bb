// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONSTANTS_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONSTANTS_H_

namespace local_discovery {

extern const char kPrivetKeyError[];
extern const char kPrivetInfoKeyToken[];
extern const char kPrivetKeyDeviceID[];
extern const char kPrivetKeyClaimURL[];
extern const char kPrivetKeyTimeout[];

extern const char kPrivetErrorDeviceBusy[];
extern const char kPrivetErrorPendingUserAction[];
extern const char kPrivetErrorInvalidXPrivetToken[];

extern const char kPrivetActionStart[];
extern const char kPrivetActionGetClaimToken[];
extern const char kPrivetActionComplete[];

const int kPrivetDefaultTimeout = 15;

const double kPrivetMaximumTimeRandomAddition = 0.2;

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_CONSTANTS_H_
