// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/feature_engagement_tracker_impl.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feature_engagement_tracker/internal/editable_configuration.h"
#include "components/feature_engagement_tracker/internal/in_memory_store.h"
#include "components/feature_engagement_tracker/internal/init_aware_model.h"
#include "components/feature_engagement_tracker/internal/model_impl.h"
#include "components/feature_engagement_tracker/internal/never_condition_validator.h"
#include "components/feature_engagement_tracker/internal/never_storage_validator.h"
#include "components/feature_engagement_tracker/internal/once_condition_validator.h"
#include "components/feature_engagement_tracker/internal/persistent_store.h"
#include "components/feature_engagement_tracker/internal/single_invalid_configuration.h"
#include "components/feature_engagement_tracker/internal/system_time_provider.h"
#include "components/feature_engagement_tracker/public/feature_constants.h"
#include "components/feature_engagement_tracker/public/feature_list.h"
#include "components/leveldb_proto/proto_database_impl.h"

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

  auto raw_model =
      base::MakeUnique<ModelImpl>(base::MakeUnique<InMemoryStore>(),
                                  base::MakeUnique<NeverStorageValidator>());

  return base::MakeUnique<FeatureEngagementTrackerImpl>(
      base::MakeUnique<InitAwareModel>(std::move(raw_model)),
      std::move(configuration), base::MakeUnique<OnceConditionValidator>(),
      base::MakeUnique<SystemTimeProvider>());
}

}  // namespace

// This method is declared in //components/feature_engagement_tracker/public/
//     feature_engagement_tracker.h
// and should be linked in to any binary using FeatureEngagementTracker::Create.
// static
FeatureEngagementTracker* FeatureEngagementTracker::Create(
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner) {
  if (base::FeatureList::IsEnabled(kIPHDemoMode))
    return CreateDemoModeFeatureEngagementTracker().release();

  std::unique_ptr<leveldb_proto::ProtoDatabase<Event>> db =
      base::MakeUnique<leveldb_proto::ProtoDatabaseImpl<Event>>(
          background_task_runner);

  auto store = base::MakeUnique<PersistentStore>(storage_dir, std::move(db));
  auto storage_validator = base::MakeUnique<NeverStorageValidator>();
  auto raw_model = base::MakeUnique<ModelImpl>(std::move(store),
                                               std::move(storage_validator));

  auto model = base::MakeUnique<InitAwareModel>(std::move(raw_model));
  auto configuration = base::MakeUnique<SingleInvalidConfiguration>();
  auto condition_validator = base::MakeUnique<NeverConditionValidator>();
  auto time_provider = base::MakeUnique<SystemTimeProvider>();

  return new FeatureEngagementTrackerImpl(
      std::move(model), std::move(configuration),
      std::move(condition_validator), std::move(time_provider));
}

FeatureEngagementTrackerImpl::FeatureEngagementTrackerImpl(
    std::unique_ptr<Model> model,
    std::unique_ptr<Configuration> configuration,
    std::unique_ptr<ConditionValidator> condition_validator,
    std::unique_ptr<TimeProvider> time_provider)
    : model_(std::move(model)),
      configuration_(std::move(configuration)),
      condition_validator_(std::move(condition_validator)),
      time_provider_(std::move(time_provider)),
      initialization_finished_(false),
      weak_ptr_factory_(this) {
  model_->Initialize(
      base::Bind(&FeatureEngagementTrackerImpl::OnModelInitializationFinished,
                 weak_ptr_factory_.GetWeakPtr()),
      time_provider_->GetCurrentDay());
}

FeatureEngagementTrackerImpl::~FeatureEngagementTrackerImpl() = default;

void FeatureEngagementTrackerImpl::NotifyEvent(const std::string& event) {
  // TODO(nyquist): Track this event in UMA.
  // TODO(nyquist): Invoke model_->IncrementEvent(...).
}

bool FeatureEngagementTrackerImpl::ShouldTriggerHelpUI(
    const base::Feature& feature) {
  // TODO(nyquist): Track this event in UMA.
  bool result =
      condition_validator_
          ->MeetsConditions(feature, configuration_->GetFeatureConfig(feature),
                            *model_, time_provider_->GetCurrentDay())
          .NoErrors();
  if (result)
    condition_validator_->NotifyIsShowing(feature);

  return result;
}

void FeatureEngagementTrackerImpl::Dismissed(const base::Feature& feature) {
  // TODO(nyquist): Track this event in UMA.
  condition_validator_->NotifyDismissed(feature);
}

bool FeatureEngagementTrackerImpl::IsInitialized() {
  return model_->IsReady();
}

void FeatureEngagementTrackerImpl::AddOnInitializedCallback(
    OnInitializedCallback callback) {
  if (initialization_finished_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, model_->IsReady()));
    return;
  }

  on_initialized_callbacks_.push_back(callback);
}

void FeatureEngagementTrackerImpl::OnModelInitializationFinished(bool success) {
  DCHECK_EQ(success, model_->IsReady());
  initialization_finished_ = true;

  for (auto& callback : on_initialized_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, success));
  }

  on_initialized_callbacks_.clear();
}

}  // namespace feature_engagement_tracker
