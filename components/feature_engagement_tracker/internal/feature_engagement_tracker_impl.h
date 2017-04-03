// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_ENGAGEMENT_TRACKER_IMPL_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_ENGAGEMENT_TRACKER_IMPL_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/feature_engagement_tracker/public/feature_engagement_tracker.h"

namespace feature_engagement_tracker {

// The internal implementation of the FeatureEngagementTracker.
class FeatureEngagementTrackerImpl : public FeatureEngagementTracker,
                                     public base::SupportsUserData {
 public:
  using FeatureVector = std::vector<const base::Feature*>;

  explicit FeatureEngagementTrackerImpl(FeatureVector features);
  ~FeatureEngagementTrackerImpl() override;

  // FeatureEngagementTracker implementation.
  void NotifyEvent(const std::string& event) override;
  bool ShouldTriggerHelpUI(const base::Feature& feature) override;
  void Dismissed() override;
  void AddOnInitializedCallback(OnInitializedCallback callback) override;

 private:
  // The list of all registerd features.
  FeatureVector features_;

  // True iff at least one feature enlightenment has been shown within the
  // current session.
  bool has_shown_enlightenment_;

  DISALLOW_COPY_AND_ASSIGN(FeatureEngagementTrackerImpl);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_ENGAGEMENT_TRACKER_IMPL_H_
