// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/feature_engagement_tracker_impl.h"

namespace feature_engagement_tracker {

// This method is declared in //components/feature_engagement_tracker/public/
//     feature_engagement_tracker.h
// and should be linked in to any binary using FeatureEngagementTracker::Create.
// static
FeatureEngagementTracker* FeatureEngagementTracker::Create(
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background__task_runner) {
  return new FeatureEngagementTrackerImpl;
}

FeatureEngagementTrackerImpl::FeatureEngagementTrackerImpl() = default;

FeatureEngagementTrackerImpl::~FeatureEngagementTrackerImpl() = default;

void FeatureEngagementTrackerImpl::Event(const std::string& feature,
                                         const std::string& precondition) {
  // TODO(nyquist): Track this event.
}

void FeatureEngagementTrackerImpl::Used(const std::string& feature) {
  // TODO(nyquist): Track this event.
}

bool FeatureEngagementTrackerImpl::Trigger(const std::string& feature) {
  // TODO(nyquist): Track this event and add business logic.
  return false;
}

void FeatureEngagementTrackerImpl::Dismissed() {
  // TODO(nyquist): Track this event.
}

void FeatureEngagementTrackerImpl::AddOnInitializedCallback(
    OnInitializedCallback callback) {
  // TODO(nyquist): Add support for this.
}

}  // namespace feature_engagement_tracker
