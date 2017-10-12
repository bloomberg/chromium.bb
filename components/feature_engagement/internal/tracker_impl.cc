// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/tracker_impl.h"

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
#include "components/feature_engagement/internal/availability_model_impl.h"
#include "components/feature_engagement/internal/chrome_variations_configuration.h"
#include "components/feature_engagement/internal/editable_configuration.h"
#include "components/feature_engagement/internal/event_model_impl.h"
#include "components/feature_engagement/internal/feature_config_condition_validator.h"
#include "components/feature_engagement/internal/feature_config_event_storage_validator.h"
#include "components/feature_engagement/internal/in_memory_event_store.h"
#include "components/feature_engagement/internal/init_aware_event_model.h"
#include "components/feature_engagement/internal/never_availability_model.h"
#include "components/feature_engagement/internal/never_event_storage_validator.h"
#include "components/feature_engagement/internal/once_condition_validator.h"
#include "components/feature_engagement/internal/persistent_event_store.h"
#include "components/feature_engagement/internal/proto/availability.pb.h"
#include "components/feature_engagement/internal/stats.h"
#include "components/feature_engagement/internal/system_time_provider.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/leveldb_proto/proto_database_impl.h"

namespace feature_engagement {

namespace {
const base::FilePath::CharType kEventDBStorageDir[] =
    FILE_PATH_LITERAL("EventDB");
const base::FilePath::CharType kAvailabilityDBStorageDir[] =
    FILE_PATH_LITERAL("AvailabilityDB");

// Creates a TrackerImpl that is usable for a demo mode.
std::unique_ptr<Tracker> CreateDemoModeTracker() {
  // GetFieldTrialParamValueByFeature returns an empty string if the param is
  // not set.
  std::string chosen_feature_name = base::GetFieldTrialParamValueByFeature(
      kIPHDemoMode, kIPHDemoModeFeatureChoiceParam);

  DVLOG(2) << "Enabling demo mode. Chosen feature: " << chosen_feature_name;

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

  auto raw_event_model = base::MakeUnique<EventModelImpl>(
      base::MakeUnique<InMemoryEventStore>(),
      base::MakeUnique<NeverEventStorageValidator>());

  return base::MakeUnique<TrackerImpl>(
      base::MakeUnique<InitAwareEventModel>(std::move(raw_event_model)),
      base::MakeUnique<NeverAvailabilityModel>(), std::move(configuration),
      base::MakeUnique<OnceConditionValidator>(),
      base::MakeUnique<SystemTimeProvider>());
}

}  // namespace

// This method is declared in //components/feature_engagement/public/
//     feature_engagement.h
// and should be linked in to any binary using Tracker::Create.
// static
Tracker* Tracker::Create(
    const base::FilePath& storage_dir,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner) {
  DVLOG(2) << "Creating Tracker";
  if (base::FeatureList::IsEnabled(kIPHDemoMode))
    return CreateDemoModeTracker().release();

  std::unique_ptr<leveldb_proto::ProtoDatabase<Event>> event_db =
      base::MakeUnique<leveldb_proto::ProtoDatabaseImpl<Event>>(
          background_task_runner);

  base::FilePath event_storage_dir = storage_dir.Append(kEventDBStorageDir);
  auto event_store = base::MakeUnique<PersistentEventStore>(
      event_storage_dir, std::move(event_db));

  auto configuration = base::MakeUnique<ChromeVariationsConfiguration>();
  configuration->ParseFeatureConfigs(GetAllFeatures());

  auto event_storage_validator =
      base::MakeUnique<FeatureConfigEventStorageValidator>();
  event_storage_validator->InitializeFeatures(GetAllFeatures(), *configuration);

  auto raw_event_model = base::MakeUnique<EventModelImpl>(
      std::move(event_store), std::move(event_storage_validator));

  auto event_model =
      base::MakeUnique<InitAwareEventModel>(std::move(raw_event_model));
  auto condition_validator =
      base::MakeUnique<FeatureConfigConditionValidator>();
  auto time_provider = base::MakeUnique<SystemTimeProvider>();

  base::FilePath availability_storage_dir =
      storage_dir.Append(kAvailabilityDBStorageDir);
  auto availability_db =
      base::MakeUnique<leveldb_proto::ProtoDatabaseImpl<Availability>>(
          background_task_runner);
  auto availability_store_loader = base::BindOnce(
      &PersistentAvailabilityStore::LoadAndUpdateStore,
      availability_storage_dir, std::move(availability_db), GetAllFeatures());

  auto availability_model = base::MakeUnique<AvailabilityModelImpl>(
      std::move(availability_store_loader));

  return new TrackerImpl(std::move(event_model), std::move(availability_model),
                         std::move(configuration),
                         std::move(condition_validator),
                         std::move(time_provider));
}

TrackerImpl::TrackerImpl(
    std::unique_ptr<EventModel> event_model,
    std::unique_ptr<AvailabilityModel> availability_model,
    std::unique_ptr<Configuration> configuration,
    std::unique_ptr<ConditionValidator> condition_validator,
    std::unique_ptr<TimeProvider> time_provider)
    : event_model_(std::move(event_model)),
      availability_model_(std::move(availability_model)),
      configuration_(std::move(configuration)),
      condition_validator_(std::move(condition_validator)),
      time_provider_(std::move(time_provider)),
      event_model_initialization_finished_(false),
      availability_model_initialization_finished_(false),
      weak_ptr_factory_(this) {
  event_model_->Initialize(
      base::Bind(&TrackerImpl::OnEventModelInitializationFinished,
                 weak_ptr_factory_.GetWeakPtr()),
      time_provider_->GetCurrentDay());

  availability_model_->Initialize(
      base::Bind(&TrackerImpl::OnAvailabilityModelInitializationFinished,
                 weak_ptr_factory_.GetWeakPtr()),
      time_provider_->GetCurrentDay());
}

TrackerImpl::~TrackerImpl() = default;

void TrackerImpl::NotifyEvent(const std::string& event) {
  event_model_->IncrementEvent(event, time_provider_->GetCurrentDay());
  stats::RecordNotifyEvent(event, configuration_.get(),
                           event_model_->IsReady());
}

bool TrackerImpl::ShouldTriggerHelpUI(const base::Feature& feature) {
  FeatureConfig feature_config = configuration_->GetFeatureConfig(feature);
  ConditionValidator::Result result = condition_validator_->MeetsConditions(
      feature, feature_config, *event_model_, *availability_model_,
      time_provider_->GetCurrentDay());
  if (result.NoErrors()) {
    condition_validator_->NotifyIsShowing(
        feature, feature_config, configuration_->GetRegisteredFeatures());

    DCHECK_NE("", feature_config.trigger.name);
    event_model_->IncrementEvent(feature_config.trigger.name,
                                 time_provider_->GetCurrentDay());
  }

  stats::RecordShouldTriggerHelpUI(feature, feature_config, result);
  DVLOG(2) << "Trigger result for " << feature.name
           << ": trigger=" << result.NoErrors()
           << " tracking_only=" << feature_config.tracking_only << " "
           << result;
  return result.NoErrors() && !feature_config.tracking_only;
}

Tracker::TriggerState TrackerImpl::GetTriggerState(
    const base::Feature& feature) {
  if (!IsInitialized()) {
    DVLOG(2) << "TriggerState for " << feature.name << ": "
             << static_cast<int>(Tracker::TriggerState::NOT_READY);
    return Tracker::TriggerState::NOT_READY;
  }

  ConditionValidator::Result result = condition_validator_->MeetsConditions(
      feature, configuration_->GetFeatureConfig(feature), *event_model_,
      *availability_model_, time_provider_->GetCurrentDay());

  if (result.trigger_ok) {
    DVLOG(2) << "TriggerState for " << feature.name << ": "
             << static_cast<int>(Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED);
    return Tracker::TriggerState::HAS_NOT_BEEN_DISPLAYED;
  }

  DVLOG(2) << "TriggerState for " << feature.name << ": "
           << static_cast<int>(Tracker::TriggerState::HAS_BEEN_DISPLAYED);
  return Tracker::TriggerState::HAS_BEEN_DISPLAYED;
}

void TrackerImpl::Dismissed(const base::Feature& feature) {
  DVLOG(2) << "Dismissing " << feature.name;
  condition_validator_->NotifyDismissed(feature);
  stats::RecordUserDismiss();
}

bool TrackerImpl::IsInitialized() {
  return event_model_->IsReady() && availability_model_->IsReady();
}

void TrackerImpl::AddOnInitializedCallback(OnInitializedCallback callback) {
  if (IsInitializationFinished()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, IsInitialized()));
    return;
  }

  on_initialized_callbacks_.push_back(callback);
}

void TrackerImpl::OnEventModelInitializationFinished(bool success) {
  DCHECK_EQ(success, event_model_->IsReady());
  event_model_initialization_finished_ = true;

  DVLOG(2) << "Event model initialization result = " << success;

  MaybePostInitializedCallbacks();
}

void TrackerImpl::OnAvailabilityModelInitializationFinished(bool success) {
  DCHECK_EQ(success, availability_model_->IsReady());
  availability_model_initialization_finished_ = true;

  DVLOG(2) << "Availability model initialization result = " << success;

  MaybePostInitializedCallbacks();
}

bool TrackerImpl::IsInitializationFinished() const {
  return event_model_initialization_finished_ &&
         availability_model_initialization_finished_;
}

void TrackerImpl::MaybePostInitializedCallbacks() {
  if (!IsInitializationFinished())
    return;

  DVLOG(2) << "Initialization finished.";

  for (auto& callback : on_initialized_callbacks_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, IsInitialized()));
  }

  on_initialized_callbacks_.clear();
}

}  // namespace feature_engagement
