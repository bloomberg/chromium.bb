// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_FEATURES_H
#define COMPONENTS_GCM_DRIVER_FEATURES_H

#include "base/feature_list.h"
//#include "components/gcm_driver/common/gcm_driver_export.h"

namespace gcm {

namespace features {

extern const base::Feature kInvalidateTokenFeature;
const int kDefaultTokenInvalidationPeriod = 0;
const char kParamNameTokenInvalidationPeriodDays[] =
    "token_invalidation_period";
const char kGroupName[] = "token_validity_data";

// The period after which the GCM token becomes stale.
base::TimeDelta GetTokenInvalidationInterval();

}  // namespace features

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_FEATURES_H
