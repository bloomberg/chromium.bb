// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/test/test_tracker.h"

#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/feature_engagement/internal/chrome_variations_configuration.h"
#include "components/feature_engagement/internal/event_model_impl.h"
#include "components/feature_engagement/internal/feature_config_condition_validator.h"
#include "components/feature_engagement/internal/feature_config_event_storage_validator.h"
#include "components/feature_engagement/internal/in_memory_event_store.h"
#include "components/feature_engagement/internal/init_aware_event_model.h"
#include "components/feature_engagement/internal/never_availability_model.h"
#include "components/feature_engagement/internal/system_time_provider.h"
#include "components/feature_engagement/internal/tracker_impl.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"

namespace feature_engagement {

// static
std::unique_ptr<Tracker> CreateTestTracker() {
  auto configuration = base::MakeUnique<ChromeVariationsConfiguration>();
  configuration->ParseFeatureConfigs(GetAllFeatures());

  auto storage_validator =
      base::MakeUnique<FeatureConfigEventStorageValidator>();
  storage_validator->InitializeFeatures(GetAllFeatures(), *configuration);

  auto raw_event_model = base::MakeUnique<EventModelImpl>(
      base::MakeUnique<InMemoryEventStore>(), std::move(storage_validator));

  auto event_model =
      base::MakeUnique<InitAwareEventModel>(std::move(raw_event_model));

  return base::MakeUnique<TrackerImpl>(
      std::move(event_model), base::MakeUnique<NeverAvailabilityModel>(),
      std::move(configuration),
      base::MakeUnique<FeatureConfigConditionValidator>(),
      base::MakeUnique<SystemTimeProvider>());
}

}  // namespace feature_engagement
