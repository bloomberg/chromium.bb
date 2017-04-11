// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PAUSE_TABS_FIELD_TRIAL_H_
#define CHROME_COMMON_PAUSE_TABS_FIELD_TRIAL_H_

#include "base/feature_list.h"

namespace pausetabs {

const base::Feature kFeature{"PauseBackgroundTabs",
                             base::FEATURE_DISABLED_BY_DEFAULT};

const char kFeatureName[] = "pause-background-tabs";

// Mode values.
const char kModeParamMinimal[] = "minimal";
const char kModeParamLow[] = "low";
const char kModeParamMedium[] = "medium";
const char kModeParamHigh[] = "high";
const char kModeParamMax[] = "max";

}  // namespace pausetabs

#endif  // CHROME_COMMON_PAUSE_TABS_FIELD_TRIAL_H_
