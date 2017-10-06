// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Feature flag to control default value for mailto: URL rewriting feature.
// Change the default value here to enable or disable this feature.
const base::Feature kMailtoUrlRewriting{"MailtoUrlRewriting",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Feature flag to prompt user to choose a mail client app. Change the default
// value here to enable or disable this feature.
const base::Feature kMailtoPromptForUserChoice{
    "MailtoPromptForUserChoice", base::FEATURE_ENABLED_BY_DEFAULT};

// Feature flag to choose whether to use Material Design style bottom sheet
// to present choices to the user. Change the default value here to enable
// to disable this feature.
const base::Feature kMailtoPromptInMdcStyle{"MailtoPromptInMdcStyle",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
