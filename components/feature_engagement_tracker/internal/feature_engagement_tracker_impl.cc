// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/feature_engagement_tracker_impl.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/feature_engagement_tracker/internal/availability_model_impl.h"
#include "components/feature_engagement_tracker/internal/chrome_variations_configuration.h"
#include "components/feature_engagement_tracker/internal/editable_configuration.h"
#include "components/feature_engagement_tracker/internal/feature_config_condition_validator.h"
#include "components/feature_engagement_tracker/internal/feature_config_storage_validator.h"
#include "components/feature_engagement_tracker/internal/in_memory_store.h"
#include "components/feature_engagement_tracker/internal/init_aware_model.h"
#include "components/feature_engagement_tracker/internal/model_impl.h"
#include "components/feature_engagement_tracker/internal/never_availability_model.h"
#include "components/feature_engagement_tracker/internal/never_storage_validator.h"
#include "components/feature_engagement_tracker/internal/once_condition_validator.h"
#include "components/feature_engagement_tracker/internal/persistent_store.h"
#include "components/feature_engagement_tracker/internal/proto/availability.pb.h"
#include "components/feature_engagement_tracker/internal/stats.h"
#include "components/feature_engagement_tracker/internal/system_time_provider.h"
#include "components/feature_engagement_tracker/public/feature_constants.h"
#include "components/feature_engagement_tracker/public/feature_list.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace feature_engagement_tracker {

namespace {
const base::FilePath::CharType kEventDBStorageDir[] =
    FILE_PATH_LITERAL("EventDB");
const base::FilePath::CharType kAvailabilityDBStorageDir[] =
    FILE_PATH_LITERAL("AvailabilityDB");

// Creates a FeatureEngagementTrackerImpl that is usable for a demo mode.
std::unique_ptr<FeatureEngagementTracker>
CreateDemoModeFeatureEngagementTracker() {
  // GetFieldTrialParamValueByFeature returns an empty string if the param is
  // not set.
  std::string chosen_feature_name = base::GetFieldTrialParamValueByFeature(
      kIPHDemoMode, kIPHDemoModeFeatureChoiceParam);

  std::unique_ptr<EditableConfiguration> configuration =
      base::MakeUnique<EditableConfiguration>();

  // Create valid configurations for all features to ensure that the
  // OnceConditionValidator acknowledges that thet meet conditions once.
  std::vector<const base::Feature*> features = GetAllFeatures();
  for (auto* feature : features) {
    // If a particular feature has been chosen to use with demo mode, only
    // mark that feature with a valid configuration.
    bool valid_config = chosen_feature_name.empty()
                            ? true
                            : chosen_feature_name == feature->name;

    FeatureConfig feature_config;
    feature_config.valid = valid_config;
    feature_config.trigger.name = feature->name + std::string("_trigger");
    configuration->SetConfiguration(feature, feature_config);
  }

  auto raw_model =
      base::MakeUnique<ModelImpl>(base::MakeUnique<InMemoryStore>(),
                                  base::MakeUnique<NeverStorageValidator>());

  return base::MakeUnique<FeatureEngagementTrackerImpl>(
      base::MakeUnique<InitAwareModel>(std::move(raw_model)),
      base::MakeUnique<NeverAvailabilityModel>(), std::move(configuration),
      base::MakeUnique<OnceConditionValidator>(),
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

  base::FilePath event_storage_dir = storage_dir.Append(kEventDBStorageDir);
  auto store =
      base::MakeUnique<PersistentStore>(event_storage_dir, std::move(db));

  auto configuration = base::MakeUnique<ChromeVariationsConfiguration>();
  configuration->ParseFeatureConfigs(GetAllFeatures());

  auto storage_validator = base::MakeUnique<FeatureConfigStorageValidator>();
  storage_validator->InitializeFeatures(GetAllFeatures(), *configuration);

  auto raw_model = base::MakeUnique<ModelImpl>(std::move(store),
                                               std::move(storage_validator));

  auto model = base::MakeUnique<InitAwareModel>(std::move(raw_model));
  auto condition_validator =
      base::MakeUnique<FeatureConfigConditionValidator>();
  auto time_provider = base::MakeUnique<SystemTimeProvider>();

  base::FilePath availability_storage_dir =
      storage_dir.Append(kAvailabilityDBStorageDir);
  auto availability_db =
      base::MakeUnique<leveldb_proto::ProtoDatabaseImpl<Availability>>(
          background_task_runner);
  auto availability_store_loader = base::BindOnce(
      &AvailabilityStore::LoadAndUpdateStore, availability_storage_dir,
      std::move(availability_db), GetAllFeatures());

  auto availability_model = base::MakeUnique<AvailabilityModelImpl>(
      std::move(availability_store_loader));

  return new FeatureEngagementTrackerImpl(
      std::move(model), std::move(availability_model), std::move(configuration),
      std::move(condition_validator), std::move(time_provider));
}

FeatureEngagementTrackerImpl::FeatureEngagementTrackerImpl(
    std::unique_ptr<Model> model,
    std::unique_ptr<AvailabilityModel> availability_model,
    std::unique_ptr<Configuration> configuration,
    std::unique_ptr<ConditionValidator> condition_validator,
    std::unique_ptr<TimeProvider> time_provider)
    : model_(std::move(model)),
      availability_model_(std::move(availability_model)),
      configuration_(std::move(configuration)),
      condition_validator_(std::move(condition_validator)),
      time_provider_(std::move(time_provider)),
      event_model_initialization_finished_(false),
      availability_model_initialization_finished_(false),
      weak_ptr_factory_(this) {
  model_->Initialize(
      base::Bind(
          &FeatureEngagementTrackerImpl::OnEventModelInitializationFinished,
          weak_ptr_factory_.GetWeakPtr()),
      time_provider_->GetCurrentDay());

  availability_model_->Initialize(
      base::Bind(&FeatureEngagementTrackerImpl::
                     OnAvailabilityModelInitializationFinished,
                 weak_ptr_factory_.GetWeakPtr()),
      time_provider_->GetCurrentDay());
}

FeatureEngagementTrackerImpl::~FeatureEngagementTrackerImpl() = default;

void FeatureEngagementTrackerImpl::NotifyEvent(const std::string& event) {
  model_->IncrementEvent(event, time_provider_->GetCurrentDay());
  stats::RecordNotifyEvent(event, configuration_.get(), model_->IsReady());
}

bool FeatureEngagementTrackerImpl::ShouldTriggerHelpUI(
    const base::Feature& feature) {
  ConditionValidator::Result result = condition_validator_->MeetsConditions(
      feature, configuration_->GetFeatureConfig(feature), *model_,
      *availability_model_, time_provider_->GetCurrentDay());
  if (result.NoErrors()) {
    condition_validator_->NotifyIsShowing(feature);
    FeatureConfig feature_config = configuration_->GetFeatureConfig(feature);
    DCHECK_NE("", feature_config.trigger.name);
    model_->IncrementEvent(feature_config.trigger.name,
                           time_provider_->GetCurrentDay());
  }

  stats::RecordShouldTriggerHelpUI(feature, result);
  DVLOG(2) << "Trigger result for " << feature.name << ": " << result;
  return result.NoErrors();
}

void FeatureEngagementTrackerImpl::Dismissed(const base::Feature& feature) {
  DVLOG(2) << "Dismissing " << feature.name;
  condition_validator_->NotifyDismissed(feature);
  stats::RecordUserDismiss();
}

bool FeatureEngagementTrackerImpl::IsInitialized() {
  return model_->IsReady() && availability_model_->IsReady();
}

void FeatureEngagementTrackerImpl::AddOnInitializedCallback(
    OnInitializedCallback callback) {
  if (IsInitializationFinished()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, IsInitialized()));
    return;
  }

  on_initialized_callbacks_.push_back(callback);
}

void FeatureEngagementTrackerImpl::OnEventModelInitializationFinished(
    bool success) {
  DCHECK_EQ(success, model_->IsReady());
  event_model_initialization_finished_ = true;

  DVLOG(2) << "Event model initialization result = " << success;

  MaybePostInitializedCallbacks();
}

void FeatureEngagementTrackerImpl::OnAvailabilityModelInitializationFinished(
    bool success) {
  DCHECK_EQ(success, availability_model_->IsReady());
  availability_model_initialization_finished_ = true;

  DVLOG(2) << "Availability model initialization result = " << success;

  MaybePostInitializedCallbacks();
}

bool FeatureEngagementTrackerImpl::IsInitializationFinished() const {
  return event_model_initialization_finished_ &&
         availability_model_initialization_finished_;
}

void FeatureEngagementTrackerImpl::MaybePostInitializedCallbacks() {
  if (!IsInitializationFinished())
    return;

  DVLOG(2) << "Initialization finished.";

  for (auto& callback : on_initialized_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, IsInitialized()));
  }

  on_initialized_callbacks_.clear();
}

}  // namespace feature_engagement_tracker
