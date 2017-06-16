// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kBrowserTaskScheduler[] = "Task Scheduler";

const char kBrowserTaskSchedulerDescription[] =
    "Enables redirection of some task posting APIs to the task scheduler.";

const char kContextualSearch[] = "Contextual Search";

const char kContextualSearchDescription[] =
    "Whether or not Contextual Search is enabled.";

const char kPhysicalWeb[] = "Physical Web";

const char kPhysicalWebDescription[] =
    "When enabled, the omnibox will include suggestions for web pages "
    "broadcast by devices near you.";

const char kMarkHttpAsName[] = "Mark non-secure origins as non-secure";

const char kMarkHttpAsDescription[] = "Change the UI treatment for HTTP pages";
const char kMarkHttpAsDangerous[] = "Always mark HTTP as actively dangerous";
const char kMarkHttpAsNonSecureAfterEditing[] =
    "Warn on HTTP after editing forms";
const char kMarkHttpAsNonSecureWhileIncognito[] =
    "Warn on HTTP while in Incognito mode";
const char kMarkHttpAsNonSecureWhileIncognitoOrEditing[] =
    "Warn on HTTP while in Incognito mode or after editing forms";

}  // namespace flag_descriptions
