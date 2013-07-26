// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// String constants used when logging data in the extension activity log.

#include "chrome/browser/extensions/activity_log/activity_action_constants.h"

namespace activity_log_constants {

// Keys that may be used in the "other" attribute of an Action.
const char kActionBlockedReason[] = "blocked_reason";
const char kActionDomVerb[] = "dom_verb";
const char kActionExtra[] = "extra";
const char kActionWebRequest[] = "web_request";

}  // namespace activity_log_constants
