// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/feature_engagement_tracker_impl.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "components/feature_engagement_tracker/internal/editable_configuration.h"
#include "components/feature_engagement_tracker/internal/feature_list.h"
#include "components/feature_engagement_tracker/internal/in_memory_store.h"
#include "components/feature_engagement_tracker/internal/model_impl.h"
#include "components/feature_engagement_tracker/internal/never_condition_validator.h"
#include "components/feature_engagement_tracker/internal/once_condition_validator.h"
#include "components/feature_engagement_tracker/internal/single_invalid_configuration.h"
#include "components/feature_engagement_tracker/public/feature_constants.h"

namespace feature_engagement_tracker {

namespace {

// Creates a FeatureEngagementTrackerImpl that is usable for a demo mode.
std::unique_ptr<FeatureEngagementTracker>
CreateDemoModeFeatureEngagementTracker() {
  std::unique_ptr<EditableConfiguration> configuration =
      base::MakeUnique<EditableConfiguration>();

  // Create valid configurations for all features to ensure that the
  // OnceConditionValidator acknowledges that thet meet conditions once.
  std::vector<const base::Feature*> features = GetAllFeatures();
  for (auto* feature : features) {
    FeatureConfig feature_config;
    feature_config.valid = true;
    configuration->SetConfiguration(feature, feature_config);
  }

  return base::MakeUnique<FeatureEngagementTrackerImpl>(
      base::MakeUnique<InMemoryStore>(), std::move(configuration),
      base::MakeUnique<OnceConditionValidator>());
}

}  // namespace

// This method is declared in //components/feature_engagement_tracker/public/
//     feature_engagement_tracker.h
// and should be linked in to any binary using FeatureEngagementTracker::Create.
// static
FeatureEngagementTracker* FeatureEngagementTracker::Create(
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background__task_runner) {
  if (base::FeatureList::IsEnabled(kIPHDemoMode))
    return CreateDemoModeFeatureEngagementTracker().release();

  std::unique_ptr<Store> store = base::MakeUnique<InMemoryStore>();
  // TODO(nyquist): Create FinchConfiguration to parse configuration.
  std::unique_ptr<Configuration> configuration =
      base::MakeUnique<SingleInvalidConfiguration>();
  std::unique_ptr<ConditionValidator> validator =
      base::MakeUnique<NeverConditionValidator>();

  return new FeatureEngagementTrackerImpl(
      std::move(store), std::move(configuration), std::move(validator));
}

FeatureEngagementTrackerImpl::FeatureEngagementTrackerImpl(
    std::unique_ptr<Store> store,
    std::unique_ptr<Configuration> configuration,
    std::unique_ptr<ConditionValidator> condition_validator)
    : condition_validator_(std::move(condition_validator)),
      weak_ptr_factory_(this) {
  model_ =
      base::MakeUnique<ModelImpl>(std::move(store), std::move(configuration));
  model_->Initialize(
      base::Bind(&FeatureEngagementTrackerImpl::OnModelInitializationFinished,
                 weak_ptr_factory_.GetWeakPtr()));
}

FeatureEngagementTrackerImpl::~FeatureEngagementTrackerImpl() = default;

void FeatureEngagementTrackerImpl::NotifyEvent(const std::string& event) {
  // TODO(nyquist): Track this event.
}

bool FeatureEngagementTrackerImpl::ShouldTriggerHelpUI(
    const base::Feature& feature) {
  // TODO(nyquist): Track this event.
  bool result = condition_validator_->MeetsConditions(feature, *model_);
  if (result)
    model_->SetIsCurrentlyShowing(true);
  return result;
}

void FeatureEngagementTrackerImpl::Dismissed() {
  // TODO(nyquist): Track this event.
  model_->SetIsCurrentlyShowing(false);
}

void FeatureEngagementTrackerImpl::AddOnInitializedCallback(
    OnInitializedCallback callback) {
  // TODO(nyquist): Add support for this.
}

void FeatureEngagementTrackerImpl::OnModelInitializationFinished(bool result) {
  // TODO(nyquist): Ensure that all OnInitializedCallbacks are invoked.
}

}  // namespace feature_engagement_tracker
