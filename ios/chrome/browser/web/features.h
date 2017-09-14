// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_FEATURES_H_
#define IOS_CHROME_BROWSER_WEB_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable rewriting of mailto: URLs to launch Mail client app
// of user's choice.
extern const base::Feature kMailtoUrlRewriting;

// Feature flag to prompt user to choose a mail client app. User is prompted
// at the first time that the user taps on a mailto: URL. This feature can be
// enabled only when kMailtoUrlRewriting is also enabled.
extern const base::Feature kMailtoPromptForUserChoice;

// Feature flag to choose whether to use Material Design style bottom sheet
// to present choices to the user. With MDC, it is possible to show a custom
// row with a toggle switch to remember the setting for future use.
extern const base::Feature kMailtoPromptInMdcStyle;

#endif  // IOS_CHROME_BROWSER_WEB_FEATURES_H_
