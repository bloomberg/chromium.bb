// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_constants.h"

namespace chromeos {
namespace supervision {

// Default URL prefix for the Supervision Onboarding pages.
const char kSupervisionServerUrlPrefix[] =
    "https://families.google.com/kids/deviceonboarding";

// Relative URL for the onboarding start page.
const char kOnboardingStartPageRelativeUrl[] = "/kids/deviceonboarding/start";

// Name of the custom HTTP header returned by the Supervision server containing
// a list of experiments that this version of the onboarding supports.
const char kExperimentHeaderName[] = "supervision-experiments";

// Experiment name for the first version of the onboarding flow.
const char kDeviceOnboardingExperimentName[] = "DeviceOnboardingV1";

}  // namespace supervision
}  // namespace chromeos
