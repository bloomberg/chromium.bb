// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_AD_INJECTION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_AD_INJECTION_UTIL_H_

namespace rappor {
class RapporService;
}

namespace extensions {

class Action;

namespace ad_injection_util {

// Check for whether or not the action may have injected ads, and if it may
// have, report it with the given |rappor_service|.
void CheckActionForAdInjection(const Action* action,
                               rappor::RapporService* rappor_service);

}  // namespace ad_injection_util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_AD_INJECTION_UTIL_H_
