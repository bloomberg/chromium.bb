// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/feature_state_manager_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

// TODO(jordynass): Use constants declared in
// chromeos/services/multidevice_setup/public/cpp/prefs.h once migration is
// complete, then delete these fields which are duplicates. See
// https://crbug.com/870065.
const char kSmartLockFeatureEnabledPrefName[] = "easy_unlock.enabled";
const char kSmartLockFeatureAllowedPrefName[] = "easy_unlock.allowed";

base::flat_map<mojom::Feature, std::string>
GenerateFeatureToEnabledPrefNameMap() {
  return base::flat_map<mojom::Feature, std::string>{
      {mojom::Feature::kBetterTogetherSuite, kSuiteEnabledPrefName},
      {mojom::Feature::kInstantTethering,
       kInstantTetheringFeatureEnabledPrefName},
      {mojom::Feature::kMessages, kAndroidMessagesFeatureEnabledPrefName},
      {mojom::Feature::kSmartLock, kSmartLockFeatureEnabledPrefName}};
}

base::flat_map<mojom::Feature, std::string>
GenerateFeatureToAllowedPrefNameMap() {
  return base::flat_map<mojom::Feature, std::string>{
      {mojom::Feature::kInstantTethering,
       kInstantTetheringFeatureAllowedPrefName},
      {mojom::Feature::kSmartLock, kSmartLockFeatureAllowedPrefName}};
}

// Each feature's default value is kUnavailableNoVerifiedHost until proven
// otherwise.
base::flat_map<mojom::Feature, mojom::FeatureState>
GenerateInitialDefaultCachedStateMap() {
  return base::flat_map<mojom::Feature, mojom::FeatureState>{
      {mojom::Feature::kBetterTogetherSuite,
       mojom::FeatureState::kUnavailableNoVerifiedHost},
      {mojom::Feature::kInstantTethering,
       mojom::FeatureState::kUnavailableNoVerifiedHost},
      {mojom::Feature::kMessages,
       mojom::FeatureState::kUnavailableNoVerifiedHost},
      {mojom::Feature::kSmartLock,
       mojom::FeatureState::kUnavailableNoVerifiedHost}};
}

}  // namespace

// static
FeatureStateManagerImpl::Factory*
    FeatureStateManagerImpl::Factory::test_factory_ = nullptr;

// static
FeatureStateManagerImpl::Factory* FeatureStateManagerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<Factory> factory;
  return factory.get();
}

// static
void FeatureStateManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

FeatureStateManagerImpl::Factory::~Factory() = default;

std::unique_ptr<FeatureStateManager>
FeatureStateManagerImpl::Factory::BuildInstance(
    PrefService* pref_service,
    HostStatusProvider* host_status_provider,
    device_sync::DeviceSyncClient* device_sync_client) {
  return base::WrapUnique(new FeatureStateManagerImpl(
      pref_service, host_status_provider, device_sync_client));
}

FeatureStateManagerImpl::FeatureStateManagerImpl(
    PrefService* pref_service,
    HostStatusProvider* host_status_provider,
    device_sync::DeviceSyncClient* device_sync_client)
    : pref_service_(pref_service),
      host_status_provider_(host_status_provider),
      device_sync_client_(device_sync_client),
      feature_to_enabled_pref_name_map_(GenerateFeatureToEnabledPrefNameMap()),
      feature_to_allowed_pref_name_map_(GenerateFeatureToAllowedPrefNameMap()),
      cached_feature_state_map_(GenerateInitialDefaultCachedStateMap()) {
  host_status_provider_->AddObserver(this);
  device_sync_client_->AddObserver(this);

  registrar_.Init(pref_service_);

  // Listen for changes to each of the "enabled" feature names.
  for (const auto& map_entry : feature_to_enabled_pref_name_map_) {
    registrar_.Add(
        map_entry.second,
        base::BindRepeating(&FeatureStateManagerImpl::OnPrefValueChanged,
                            base::Unretained(this)));
  }

  // Also listen for changes to each of the "allowed" feature names.
  for (const auto& map_entry : feature_to_allowed_pref_name_map_) {
    registrar_.Add(
        map_entry.second,
        base::BindRepeating(&FeatureStateManagerImpl::OnPrefValueChanged,
                            base::Unretained(this)));
  }

  // Prime the cache. Since this is the initial computation, it does not
  // represent a true change of feature state values, so observers should not be
  // notified.
  UpdateFeatureStateCache(false /* notify_observers_of_changes */);
}

FeatureStateManagerImpl::~FeatureStateManagerImpl() {
  host_status_provider_->RemoveObserver(this);
  device_sync_client_->RemoveObserver(this);
}

FeatureStateManager::FeatureStatesMap
FeatureStateManagerImpl::GetFeatureStates() {
  return cached_feature_state_map_;
}

void FeatureStateManagerImpl::PerformSetFeatureEnabledState(
    mojom::Feature feature,
    bool enabled) {
  // Note: Since |registrar_| observes changes to all relevant preferences,
  // this call will result in OnPrefValueChanged() being invoked, resulting in
  // observers being notified of the change.
  pref_service_->SetBoolean(feature_to_enabled_pref_name_map_[feature],
                            enabled);
}

void FeatureStateManagerImpl::OnHostStatusChange(
    const HostStatusProvider::HostStatusWithDevice& host_status_with_device) {
  UpdateFeatureStateCache(true /* notify_observers_of_changes */);
}

void FeatureStateManagerImpl::OnNewDevicesSynced() {
  UpdateFeatureStateCache(true /* notify_observers_of_changes */);
}

void FeatureStateManagerImpl::OnPrefValueChanged() {
  UpdateFeatureStateCache(true /* notify_observers_of_changes */);
}

void FeatureStateManagerImpl::UpdateFeatureStateCache(
    bool notify_observers_of_changes) {
  // Make a copy of |cached_feature_state_map_| before making edits to it.
  FeatureStatesMap previous_cached_feature_state_map =
      cached_feature_state_map_;

  // Update |cached_feature_state_map_|.
  auto it = cached_feature_state_map_.begin();
  while (it != cached_feature_state_map_.end()) {
    it->second = ComputeFeatureState(it->first);
    ++it;
  }

  if (previous_cached_feature_state_map == cached_feature_state_map_)
    return;

  NotifyFeatureStatesChange(cached_feature_state_map_);
}

mojom::FeatureState FeatureStateManagerImpl::ComputeFeatureState(
    mojom::Feature feature) {
  HostStatusProvider::HostStatusWithDevice status_with_device =
      host_status_provider_->GetHostWithStatus();

  if (status_with_device.host_status() != mojom::HostStatus::kHostVerified)
    return mojom::FeatureState::kUnavailableNoVerifiedHost;

  if (!IsSupportedByChromebook(feature))
    return mojom::FeatureState::kNotSupportedByChromebook;

  if (!HasSufficientSecurity(feature, *status_with_device.host_device()))
    return mojom::FeatureState::kUnavailableInsufficientSecurity;

  if (!HasBeenActivatedByPhone(feature, *status_with_device.host_device()))
    return mojom::FeatureState::kNotSupportedByPhone;

  return GetEnabledOrDisabledState(feature);
}

bool FeatureStateManagerImpl::IsSupportedByChromebook(mojom::Feature feature) {
  static const std::pair<mojom::Feature, cryptauth::SoftwareFeature>
      kFeatureAndClientSoftwareFeaturePairs[] = {
          {mojom::Feature::kBetterTogetherSuite,
           cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT},
          {mojom::Feature::kInstantTethering,
           cryptauth::SoftwareFeature::MAGIC_TETHER_CLIENT},
          {mojom::Feature::kMessages,
           cryptauth::SoftwareFeature::SMS_CONNECT_CLIENT},
          {mojom::Feature::kSmartLock,
           cryptauth::SoftwareFeature::EASY_UNLOCK_CLIENT}};

  for (const auto& pair : kFeatureAndClientSoftwareFeaturePairs) {
    if (pair.first != feature)
      continue;

    return device_sync_client_->GetLocalDeviceMetadata()
               ->GetSoftwareFeatureState(pair.second) !=
           cryptauth::SoftwareFeatureState::kNotSupported;
  }

  NOTREACHED();
  return false;
}

bool FeatureStateManagerImpl::HasSufficientSecurity(
    mojom::Feature feature,
    const cryptauth::RemoteDeviceRef& host_device) {
  if (feature != mojom::Feature::kSmartLock)
    return true;

  // Special case for Smart Lock: if the host device does not have a lock screen
  // set, its SoftwareFeatureState for EASY_UNLOCK_HOST is supported but not
  // enabled.
  return host_device.GetSoftwareFeatureState(
             cryptauth::SoftwareFeature::EASY_UNLOCK_HOST) !=
         cryptauth::SoftwareFeatureState::kSupported;
}

bool FeatureStateManagerImpl::HasBeenActivatedByPhone(
    mojom::Feature feature,
    const cryptauth::RemoteDeviceRef& host_device) {
  static const std::pair<mojom::Feature, cryptauth::SoftwareFeature>
      kFeatureAndHostSoftwareFeaturePairs[] = {
          {mojom::Feature::kBetterTogetherSuite,
           cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST},
          {mojom::Feature::kInstantTethering,
           cryptauth::SoftwareFeature::MAGIC_TETHER_HOST},
          {mojom::Feature::kMessages,
           cryptauth::SoftwareFeature::SMS_CONNECT_HOST},
          {mojom::Feature::kSmartLock,
           cryptauth::SoftwareFeature::EASY_UNLOCK_HOST}};

  for (const auto& pair : kFeatureAndHostSoftwareFeaturePairs) {
    if (pair.first != feature)
      continue;

    return host_device.GetSoftwareFeatureState(pair.second) ==
           cryptauth::SoftwareFeatureState::kEnabled;
  }

  NOTREACHED();
  return false;
}

mojom::FeatureState FeatureStateManagerImpl::GetEnabledOrDisabledState(
    mojom::Feature feature) {
  // If an "allowed pref" exists for this feature and that preference is set to
  // false, the feature is disabled by policy.
  if (base::ContainsKey(feature_to_allowed_pref_name_map_, feature) &&
      !pref_service_->GetBoolean(feature_to_allowed_pref_name_map_[feature])) {
    return mojom::FeatureState::kDisabledByPolicy;
  }

  if (!base::ContainsKey(feature_to_enabled_pref_name_map_, feature)) {
    PA_LOG(ERROR) << "FeatureStateManagerImpl::GetEnabledOrDisabledState(): "
                  << "Feature not present in \"enabled pref\" map: " << feature;
    NOTREACHED();
  }

  return pref_service_->GetBoolean(feature_to_enabled_pref_name_map_[feature])
             ? mojom::FeatureState::kEnabledByUser
             : mojom::FeatureState::kDisabledByUser;
}

}  // namespace multidevice_setup

}  // namespace chromeos
