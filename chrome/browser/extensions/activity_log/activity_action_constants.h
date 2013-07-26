// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// String constants used when logging data in the extension activity log.

#ifndef CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTION_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTION_CONSTANTS_H_

namespace activity_log_constants {

// Keys that may be used in the "other" attribute of an Action.
extern const char kActionBlockedReason[];
extern const char kActionDomVerb[];
extern const char kActionExtra[];
extern const char kActionWebRequest[];

}  // namespace activity_log_constants

#endif  // CHROME_BROWSER_EXTENSIONS_ACTIVITY_LOG_ACTIVITY_ACTION_CONSTANTS_H_
