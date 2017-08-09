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

const char kUseDdljsonApiName[] = "Use new ddljson API for Doodles";
const char kUseDdljsonApiDescription[] =
    "Enables the new ddljson API to fetch Doodles for the NTP.";

const char kWebPaymentsName[] = "Web Payments";

const char kWebPaymentsDescription[] =
    "Enable Payment Request API integration, a JavaScript API for merchants.";

const char kWebPaymentsNativeAppsName[] = "Web Payments Native Apps";
const char kWebPaymentsNativeAppsDescription[] =
    "Enable third party iOS native apps as payments methods within Payment "
    "Request.";

const char kCaptivePortalName[] = "Captive Portal";
const char kCaptivePortalDescription[] =
    "When enabled, the Captive Portal landing page will be displayed if it is "
    "detected that the user is connected to a Captive Portal network.";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

}  // namespace flag_descriptions
