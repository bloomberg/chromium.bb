// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/constants.h"

#include "base/feature_list.h"

namespace nux {

const base::Feature kNuxOnboardingFeature{"NuxOnboarding",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// The value of these FeatureParam values should be a comma-delimited list
// of element names whitelisted in the MODULES_WHITELIST list, defined in
// chrome/browser/resources/welcome/onboarding_welcome/welcome_app.js
const base::FeatureParam<std::string> kNuxOnboardingNewUserModules{
    &kNuxOnboardingFeature, "new-user-modules",
    "nux-google-apps,nux-email,nux-set-as-default,signin-view"};
const base::FeatureParam<std::string> kNuxOnboardingReturningUserModules{
    &kNuxOnboardingFeature, "returning-user-modules", "nux-set-as-default"};

}  // namespace nux
