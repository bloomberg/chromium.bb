// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_ENGAGEMENT_TRACKER_IMPL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_ENGAGEMENT_TRACKER_IMPL_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "components/feature_engagement_tracker/internal/condition_validator.h"
#include "components/feature_engagement_tracker/internal/model.h"
#include "components/feature_engagement_tracker/public/feature_engagement_tracker.h"

namespace feature_engagement_tracker {
class Store;

// The internal implementation of the FeatureEngagementTracker.
class FeatureEngagementTrackerImpl : public FeatureEngagementTracker,
                                     public base::SupportsUserData {
 public:
  FeatureEngagementTrackerImpl(
      std::unique_ptr<Store> store,
      std::unique_ptr<Configuration> configuration,
      std::unique_ptr<ConditionValidator> condition_validator);
  ~FeatureEngagementTrackerImpl() override;

  // FeatureEngagementTracker implementation.
  void NotifyEvent(const std::string& event) override;
  bool ShouldTriggerHelpUI(const base::Feature& feature) override;
  void Dismissed() override;
  void AddOnInitializedCallback(OnInitializedCallback callback) override;

 private:
  // Invoked by the Model when it has been initialized.
  void OnModelInitializationFinished(bool result);

  // The current model.
  std::unique_ptr<Model> model_;

  // The ConditionValidator provides functionality for knowing when to trigger
  // help UI.
  std::unique_ptr<ConditionValidator> condition_validator_;

  base::WeakPtrFactory<FeatureEngagementTrackerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeatureEngagementTrackerImpl);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_ENGAGEMENT_TRACKER_IMPL_H_
