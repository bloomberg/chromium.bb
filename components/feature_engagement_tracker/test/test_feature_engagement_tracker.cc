// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/test/test_feature_engagement_tracker.h"

#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/feature_engagement_tracker/internal/chrome_variations_configuration.h"
#include "components/feature_engagement_tracker/internal/feature_config_condition_validator.h"
#include "components/feature_engagement_tracker/internal/feature_config_storage_validator.h"
#include "components/feature_engagement_tracker/internal/feature_engagement_tracker_impl.h"
#include "components/feature_engagement_tracker/internal/in_memory_store.h"
#include "components/feature_engagement_tracker/internal/init_aware_model.h"
#include "components/feature_engagement_tracker/internal/model_impl.h"
#include "components/feature_engagement_tracker/internal/never_availability_model.h"
#include "components/feature_engagement_tracker/internal/system_time_provider.h"
#include "components/feature_engagement_tracker/public/feature_engagement_tracker.h"
#include "components/feature_engagement_tracker/public/feature_list.h"

namespace feature_engagement_tracker {

// static
std::unique_ptr<FeatureEngagementTracker> CreateTestFeatureEngagementTracker() {
  auto configuration = base::MakeUnique<ChromeVariationsConfiguration>();
  configuration->ParseFeatureConfigs(GetAllFeatures());

  auto storage_validator = base::MakeUnique<FeatureConfigStorageValidator>();
  storage_validator->InitializeFeatures(GetAllFeatures(), *configuration);

  auto raw_model = base::MakeUnique<ModelImpl>(
      base::MakeUnique<InMemoryStore>(), std::move(storage_validator));

  auto model = base::MakeUnique<InitAwareModel>(std::move(raw_model));

  return base::MakeUnique<FeatureEngagementTrackerImpl>(
      std::move(model), base::MakeUnique<NeverAvailabilityModel>(),
      std::move(configuration),
      base::MakeUnique<FeatureConfigConditionValidator>(),
      base::MakeUnique<SystemTimeProvider>());
}

}  // namespace feature_engagement_tracker
