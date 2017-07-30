// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_FEATURES_H_
#define COMPONENTS_SAFE_BROWSING_FEATURES_H_

#include <stddef.h>
#include <algorithm>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/values.h"
namespace base {
class ListValue;
}  // namespace base

namespace safe_browsing {
// Features list
extern const base::Feature kAdSamplerTriggerFeature;
extern const base::Feature kGoogleBrandedPhishingWarning;
extern const base::Feature kLocalDatabaseManagerEnabled;
extern const base::Feature kPasswordFieldOnFocusPinging;
extern const base::Feature kPasswordProtectionInterstitial;
extern const base::Feature kProtectedPasswordEntryPinging;
extern const base::Feature kThreatDomDetailsTagAndAttributeFeature;
extern const base::Feature kV4OnlyEnabled;

base::ListValue GetFeatureStatusList();

#endif  // COMPONENTS_SAFE_BROWSING_FEATURES_H_
}  // namespace safe_browsing
