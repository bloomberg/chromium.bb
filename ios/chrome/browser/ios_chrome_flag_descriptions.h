// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_

namespace flag_descriptions {

// Title for the flag to control redirection to the task scheduler.
extern const char kBrowserTaskScheduler[];

// Description of about:flag option to control redirection to the task
// scheduler.
extern const char kBrowserTaskSchedulerDescription[];

// Title for the flag to enable Contextual Search.
extern const char kContextualSearch[];

// Description for the flag to enable Contextual Search.
extern const char kContextualSearchDescription[];

// Title, description, and options for the MarkHttpAs setting that controls
// display of omnibox warnings about non-secure pages.
extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];
extern const char kMarkHttpAsDangerous[];
extern const char kMarkHttpAsNonSecureAfterEditing[];
extern const char kMarkHttpAsNonSecureWhileIncognito[];
extern const char kMarkHttpAsNonSecureWhileIncognitoOrEditing[];

// Title for the flag to enable Physical Web in the omnibox.
extern const char kPhysicalWeb[];

// Description for the flag to enable Physical Web in the omnibox.
extern const char kPhysicalWebDescription[];

// Title and description for the flag to enable the ddljson Doodle API.
extern const char kUseDdljsonApiName[];
extern const char kUseDdljsonApiDescription[];

// Title and description for the flag to enable Web Payments.
extern const char kWebPaymentsName[];
extern const char kWebPaymentsDescription[];

// Title and description for the flag to enable third party payment app
// integration with Web Payments.
extern const char kWebPaymentsNativeAppsName[];
extern const char kWebPaymentsNativeAppsDescription[];

// Title and description for the flag to enable Captive Portal Login.
extern const char kCaptivePortalName[];
extern const char kCaptivePortalDescription[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title and description for the flag to enable elision of the URL path, query,
// and ref in omnibox URL suggestions.
extern const char kOmniboxUIElideSuggestionUrlAfterHostName[];
extern const char kOmniboxUIElideSuggestionUrlAfterHostDescription[];

// Title and description for the flag to enable hiding the URL scheme in
// omnibox URL suggestions.
extern const char kOmniboxUIHideSuggestionUrlSchemeName[];
extern const char kOmniboxUIHideSuggestionUrlSchemeDescription[];

// Title and description for the flag to enable hiding trivial subdomains
// (www, m) in omnibox URL suggestions.
extern const char kOmniboxUIHideSuggestionUrlTrivialSubdomainsName[];
extern const char kOmniboxUIHideSuggestionUrlTrivialSubdomainsDescription[];

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_
