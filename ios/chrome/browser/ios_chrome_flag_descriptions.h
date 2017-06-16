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

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_FLAG_DESCRIPTIONS_H_
