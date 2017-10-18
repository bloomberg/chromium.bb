// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_

namespace flag_descriptions {

// Title and description for the flag to enable the new bookmark UI.
extern const char kBookmarkNewGenerationName[];
extern const char kBookmarkNewGenerationDescription[];

// Title and description for the flag to control redirection to the task
// scheduler.
extern const char kBrowserTaskScheduler[];
extern const char kBrowserTaskSchedulerDescription[];

// Title and description for the flag to enable Captive Portal Login.
extern const char kCaptivePortalName[];
extern const char kCaptivePortalDescription[];

// Title and description for the flag to enable Contextual Search.
extern const char kContextualSearch[];
extern const char kContextualSearchDescription[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title, description, and options for the MarkHttpAs setting that controls
// display of omnibox warnings about non-secure pages.
extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];
extern const char kMarkHttpAsDangerous[];

// Title and descript for the flag to enable a prompt asking user to choose
// which mail client app to use to handle mailto:// URLs.
extern const char kMailtoPromptForUserChoiceName[];
extern const char kMailtoPromptForUserChoiceDescription[];

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

// Title and description for the flag to enable Physical Web in the omnibox.
extern const char kPhysicalWeb[];
extern const char kPhysicalWebDescription[];

// Title and description for the flag to have the toolbar respect the safe area.
extern const char kSafeAreaCompatibleToolbarName[];
extern const char kSafeAreaCompatibleToolbarDescription[];

// Title and description for the flag to enable the TabSwitcher to present the
// BVC.
extern const char kTabSwitcherPresentsBVCName[];
extern const char kTabSwitcherPresentsBVCDescription[];

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

// Please insert your name/description above in alphabetical order.

// Title and description for the flag to enable drag and drop.
extern const char kDragAndDropName[];
extern const char kDragAndDropDescription[];

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_
