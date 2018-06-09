// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/observer_notifier.h"

#include <set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/time/clock.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "chromeos/services/multidevice_setup/setup_flow_completion_recorder.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/cryptauth/software_feature_state.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

const int64_t kTimestampNotSet = 0;
const char kNoHost[] = "";

base::Optional<std::string> GetHostPublicKey(
    const cryptauth::RemoteDeviceRefList& device_ref_list) {
  for (const auto& device_ref : device_ref_list) {
    if (device_ref.GetSoftwareFeatureState(
            cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST) ==
        cryptauth::SoftwareFeatureState::kEnabled) {
      DCHECK(!device_ref.public_key().empty());
      return device_ref.public_key();
    }
  }
  return base::nullopt;
}

}  // namespace

// static
ObserverNotifier::Factory* ObserverNotifier::Factory::test_factory_ = nullptr;

// static
ObserverNotifier::Factory* ObserverNotifier::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<ObserverNotifier::Factory> factory;
  return factory.get();
}

// static
void ObserverNotifier::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

ObserverNotifier::Factory::~Factory() = default;

std::unique_ptr<ObserverNotifier> ObserverNotifier::Factory::BuildInstance(
    device_sync::DeviceSyncClient* device_sync_client,
    PrefService* pref_service,
    SetupFlowCompletionRecorder* setup_flow_completion_recorder,
    base::Clock* clock) {
  return base::WrapUnique(new ObserverNotifier(
      device_sync_client, pref_service, setup_flow_completion_recorder, clock));
}

// static
void ObserverNotifier::RegisterPrefs(PrefRegistrySimple* registry) {
  // Records the timestamps (in milliseconds since UNIX Epoch, aka JavaTime) of
  // the last instance the observer was notified for each of the changes listed
  // in the class description.
  registry->RegisterInt64Pref(kNewUserPotentialHostExistsPrefName,
                              kTimestampNotSet);
  registry->RegisterInt64Pref(kExistingUserHostSwitchedPrefName,
                              kTimestampNotSet);
  registry->RegisterInt64Pref(kExistingUserChromebookAddedPrefName,
                              kTimestampNotSet);
  registry->RegisterStringPref(kHostPublicKeyFromMostRecentSyncPrefName,
                               kNoHost);
}

ObserverNotifier::~ObserverNotifier() {
  device_sync_client_->RemoveObserver(this);
}

void ObserverNotifier::SetMultiDeviceSetupObserverPtr(
    mojom::MultiDeviceSetupObserverPtr observer_ptr) {
  if (observer_ptr_.is_bound()) {
    PA_LOG(ERROR) << "There is already a mojom::MultiDeviceSetupObserverPtr "
                  << "set. There should only be one.";
    NOTREACHED();
  }
  observer_ptr_ = std::move(observer_ptr);
  CheckForMultiDeviceEvents();
}

// static
const char ObserverNotifier::kNewUserPotentialHostExistsPrefName[] =
    "multidevice_setup.new_user_potential_host_exists";

// static
const char ObserverNotifier::kExistingUserHostSwitchedPrefName[] =
    "multidevice_setup.existing_user_host_switched";

// static
const char ObserverNotifier::kExistingUserChromebookAddedPrefName[] =
    "multidevice_setup.existing_user_chromebook_added";

// static
const char ObserverNotifier::kHostPublicKeyFromMostRecentSyncPrefName[] =
    "multidevice_setup.host_public_key_from_most_recent_sync";

ObserverNotifier::ObserverNotifier(
    device_sync::DeviceSyncClient* device_sync_client,
    PrefService* pref_service,
    SetupFlowCompletionRecorder* setup_flow_completion_recorder,
    base::Clock* clock)
    : device_sync_client_(device_sync_client),
      pref_service_(pref_service),
      setup_flow_completion_recorder_(setup_flow_completion_recorder),
      clock_(clock) {
  host_public_key_from_most_recent_sync_ =
      LoadHostPublicKeyFromEndOfPreviousSession();
  cryptauth::RemoteDeviceRefList device_ref_list =
      device_sync_client_->GetSyncedDevices();
  base::Optional<cryptauth::RemoteDeviceRef> local_device =
      device_sync_client_->GetLocalDeviceMetadata();
  // ObserverNotifier should not be constructed if DeviceSyncClient is not
  // initialized.
  DCHECK(local_device);
  local_device_is_enabled_client_ =
      local_device->GetSoftwareFeatureState(
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT) ==
      cryptauth::SoftwareFeatureState::kEnabled;
  device_sync_client_->AddObserver(this);
}

void ObserverNotifier::OnNewDevicesSynced() {
  CheckForMultiDeviceEvents();
}

void ObserverNotifier::CheckForMultiDeviceEvents() {
  // Without an observer, there is nothing to do.
  if (!observer_ptr_) {
    PA_LOG(INFO) << "You must set a mojom::MultiDeviceSetupObserverPtr using "
                 << "ObserverNotifier::SetMultiDeviceSetupObserverPtr()";
    return;
  }

  cryptauth::RemoteDeviceRefList device_ref_list =
      device_sync_client_->GetSyncedDevices();

  // Track and update host info.
  base::Optional<std::string> host_public_key_before_sync =
      host_public_key_from_most_recent_sync_;
  host_public_key_from_most_recent_sync_ = GetHostPublicKey(device_ref_list);
  if (host_public_key_from_most_recent_sync_)
    pref_service_->SetString(kHostPublicKeyFromMostRecentSyncPrefName,
                             *host_public_key_from_most_recent_sync_);

  // Track and update local client info.
  bool local_device_was_enabled_client_before_sync =
      local_device_is_enabled_client_;
  local_device_is_enabled_client_ =
      device_sync_client_->GetLocalDeviceMetadata()->GetSoftwareFeatureState(
          cryptauth::SoftwareFeature::BETTER_TOGETHER_CLIENT) ==
      cryptauth::SoftwareFeatureState::kEnabled;

  CheckForNewUserPotentialHostExistsEvent(device_ref_list);
  CheckForExistingUserHostSwitchedEvent(host_public_key_before_sync);
  CheckForExistingUserChromebookAddedEvent(
      local_device_was_enabled_client_before_sync);
}

void ObserverNotifier::CheckForNewUserPotentialHostExistsEvent(
    const cryptauth::RemoteDeviceRefList& device_ref_list) {
  // We only check for new user events if there is no enabled host.
  if (host_public_key_from_most_recent_sync_)
    return;

  // If the observer has been notified of this event before, the user is not
  // new.
  if (pref_service_->GetInt64(kNewUserPotentialHostExistsPrefName) !=
      kTimestampNotSet)
    return;

  for (const auto& device_ref : device_ref_list) {
    if (device_ref.GetSoftwareFeatureState(
            cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST) !=
        cryptauth::SoftwareFeatureState::kSupported)
      continue;

    observer_ptr_->OnPotentialHostExistsForNewUser();
    pref_service_->SetInt64(kNewUserPotentialHostExistsPrefName,
                            clock_->Now().ToJavaTime());
    return;
  }
}

void ObserverNotifier::CheckForExistingUserHostSwitchedEvent(
    base::Optional<std::string> host_public_key_before_sync) {
  // If the local device is not an enabled client, the account's new host is not
  // yet the local device's new host.
  if (!local_device_is_enabled_client_)
    return;

  // The host switched event requires both a pre-sync and a post-sync host.
  if (!host_public_key_from_most_recent_sync_ || !host_public_key_before_sync)
    return;

  // If the host stayed the same, there was no switch.
  if (host_public_key_before_sync == host_public_key_from_most_recent_sync_)
    return;

  observer_ptr_->OnConnectedHostSwitchedForExistingUser();
  pref_service_->SetInt64(kExistingUserHostSwitchedPrefName,
                          clock_->Now().ToJavaTime());
}

void ObserverNotifier::CheckForExistingUserChromebookAddedEvent(
    bool local_device_was_enabled_client_before_sync) {
  // The chromebook added event requires that the local device changed its
  // client status in the sync from not being enabled to being enabled.
  if (!local_device_is_enabled_client_ ||
      local_device_was_enabled_client_before_sync)
    return;

  // This event only applies if the user completed the setup flow on a different
  // device.
  if (setup_flow_completion_recorder_->GetCompletionTimestamp())
    return;

  // Without an enabled host, the local device cannot be an enabled client.
  DCHECK(host_public_key_from_most_recent_sync_);

  observer_ptr_->OnNewChromebookAddedForExistingUser();
  pref_service_->SetInt64(kExistingUserChromebookAddedPrefName,
                          clock_->Now().ToJavaTime());
}

void ObserverNotifier::FlushForTesting() {
  if (observer_ptr_)
    observer_ptr_.FlushForTesting();
}

base::Optional<std::string>
ObserverNotifier::LoadHostPublicKeyFromEndOfPreviousSession() {
  std::string host_public_key_from_most_recent_sync =
      pref_service_->GetString(kHostPublicKeyFromMostRecentSyncPrefName);
  if (host_public_key_from_most_recent_sync.empty())
    return base::nullopt;
  return host_public_key_from_most_recent_sync;
}

}  // namespace multidevice_setup

}  // namespace chromeos
