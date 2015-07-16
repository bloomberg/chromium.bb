// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_VARIATIONS_VARIATIONS_UTIL_H_
#define CHROME_COMMON_VARIATIONS_VARIATIONS_UTIL_H_

#include <string>

namespace chrome_variations {

struct FieldTrialTestingConfig;

// Get the current set of chosen FieldTrial groups (aka variations) and send
// them to the child process logging module so it can save it for crash dumps.
void SetChildProcessLoggingVariationList();

// Provides a mechanism to associate multiple set of params to multiple groups
// with a formatted string specified from commandline. See
// kForceFieldTrialParams in chrome/common/chrome_switches.cc for more details
// on the formatting.
bool AssociateParamsFromString(const std::string& variations_string);

// Provides a mechanism to associate multiple set of params to multiple groups
// with the |config| struct. This will also force the selection of FieldTrial
// groups specified in the |config|.
void AssociateParamsFromFieldTrialConfig(const FieldTrialTestingConfig& config);

// Associates params to FieldTrial groups and forces the selection of groups
// specified in testing/variations/fieldtrial_testing_config_*.json.
void AssociateDefaultFieldTrialConfig();

}  // namespace chrome_variations

#endif  // CHROME_COMMON_VARIATIONS_VARIATIONS_UTIL_H_
