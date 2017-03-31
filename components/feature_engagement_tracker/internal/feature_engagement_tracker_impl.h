// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_ENGAGEMENT_TRACKER_IMPL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_ENGAGEMENT_TRACKER_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/feature_engagement_tracker/public/feature_engagement_tracker.h"

namespace feature_engagement_tracker {

// The internal implementation of the FeatureEngagementTracker.
class FeatureEngagementTrackerImpl : public FeatureEngagementTracker,
                                     public base::SupportsUserData {
 public:
  FeatureEngagementTrackerImpl();
  ~FeatureEngagementTrackerImpl() override;

  // FeatureEngagementTracker implementation.
  void Event(const std::string& feature,
             const std::string& precondition) override;
  void Used(const std::string& feature) override;
  bool Trigger(const std::string& feature) override;
  void Dismissed() override;
  void AddOnInitializedCallback(OnInitializedCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeatureEngagementTrackerImpl);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_ENGAGEMENT_TRACKER_IMPL_H_
