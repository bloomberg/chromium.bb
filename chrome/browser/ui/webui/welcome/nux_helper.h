// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_HELPER_H_

#include "base/macros.h"
#include "base/metrics/field_trial_params.h"

namespace base {
class DictionaryValue;
struct Feature;
}  // namespace base

class Profile;

namespace nux {
extern const base::Feature kNuxOnboardingForceEnabled;

extern const base::FeatureParam<std::string>
    kNuxOnboardingForceEnabledNewUserModules;
extern const base::FeatureParam<std::string>
    kNuxOnboardingForceEnabledReturningUserModules;

bool IsNuxOnboardingEnabled(Profile* profile);

base::DictionaryValue GetNuxOnboardingModules(Profile* profile);
}  // namespace nux

#endif  // CHROME_BROWSER_UI_WEBUI_WELCOME_NUX_HELPER_H_
