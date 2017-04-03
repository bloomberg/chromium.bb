// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/feature_engagement_tracker_impl.h"

#include "base/feature_list.h"
#include "components/feature_engagement_tracker/internal/feature_list.h"
#include "components/feature_engagement_tracker/public/feature_constants.h"

namespace feature_engagement_tracker {

// This method is declared in //components/feature_engagement_tracker/public/
//     feature_engagement_tracker.h
// and should be linked in to any binary using FeatureEngagementTracker::Create.
// static
FeatureEngagementTracker* FeatureEngagementTracker::Create(
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background__task_runner) {
  return new FeatureEngagementTrackerImpl(GetAllFeatures());
}

FeatureEngagementTrackerImpl::FeatureEngagementTrackerImpl(
    FeatureVector features)
    : features_(features), has_shown_enlightenment_(false) {}

FeatureEngagementTrackerImpl::~FeatureEngagementTrackerImpl() = default;

void FeatureEngagementTrackerImpl::Event(const base::Feature& feature,
                                         const std::string& precondition) {
  // TODO(nyquist): Track this event.
}

void FeatureEngagementTrackerImpl::Used(const base::Feature& feature) {
  // TODO(nyquist): Track this event.
}

bool FeatureEngagementTrackerImpl::Trigger(const base::Feature& feature) {
  bool should_trigger =
      !has_shown_enlightenment_ && base::FeatureList::IsEnabled(feature);
  has_shown_enlightenment_ |= should_trigger;
  return should_trigger;
}

void FeatureEngagementTrackerImpl::Dismissed() {
  // TODO(nyquist): Track this event.
}

void FeatureEngagementTrackerImpl::AddOnInitializedCallback(
    OnInitializedCallback callback) {
  // TODO(nyquist): Add support for this.
}

}  // namespace feature_engagement_tracker
