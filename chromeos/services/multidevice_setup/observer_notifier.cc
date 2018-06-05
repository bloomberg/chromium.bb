// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/observer_notifier.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/time/clock.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"
#include "components/cryptauth/remote_device_ref.h"
#include "components/cryptauth/software_feature_state.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {

namespace {
// static
bool IsNewUserWithPotentialHosts(
    const cryptauth::RemoteDeviceRefList& device_ref_list) {
  bool does_potential_host_exist = false;
  for (const auto& device_ref : device_ref_list) {
    // Checks if the user already enabled the features (i.e. not a new user).
    if (device_ref.GetSoftwareFeatureState(
            cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST) ==
        cryptauth::SoftwareFeatureState::kEnabled) {
      return false;
    }
    does_potential_host_exist |=
        device_ref.GetSoftwareFeatureState(
            cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST) ==
        cryptauth::SoftwareFeatureState::kSupported;
  }
  return does_potential_host_exist;
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

std::unique_ptr<ObserverNotifier> ObserverNotifier::Factory::BuildInstance(
    device_sync::DeviceSyncClient* device_sync_client,
    PrefService* pref_service,
    base::Clock* clock) {
  return base::WrapUnique(
      new ObserverNotifier(device_sync_client, pref_service, clock));
}

// static
void ObserverNotifier::RegisterPrefs(PrefRegistrySimple* registry) {
  // Records the timestamps (in milliseconds since UNIX Epoch, aka JavaTime) of
  // the last instance the observer was notified for each of the changes listed
  // in the class description.
  registry->RegisterInt64Pref(kNewUserPotentialHostExists, 0);
  registry->RegisterInt64Pref(kExistingUserHostSwitched, 0);
  registry->RegisterInt64Pref(kExistingUserChromebookAdded, 0);
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
  CheckForEligibleDevices();
}

// static
const char ObserverNotifier::kNewUserPotentialHostExists[] =
    "multidevice_setup.new_user_potential_host_exists";

// static
const char ObserverNotifier::kExistingUserHostSwitched[] =
    "multidevice_setup.existing_user_host_switched";

// static
const char ObserverNotifier::kExistingUserChromebookAdded[] =
    "multidevice_setup.existing_user_chromebook_added";

ObserverNotifier::ObserverNotifier(
    device_sync::DeviceSyncClient* device_sync_client,
    PrefService* pref_service,
    base::Clock* clock)
    : device_sync_client_(device_sync_client),
      pref_service_(pref_service),
      clock_(clock) {
  device_sync_client_->AddObserver(this);
}

void ObserverNotifier::OnNewDevicesSynced() {
  CheckForEligibleDevices();
}

void ObserverNotifier::CheckForEligibleDevices() {
  // Without an observer, there is nothing to do.
  if (!observer_ptr_) {
    PA_LOG(ERROR) << "You must set a mojom::MultiDeviceSetupObserverPtr (use "
                  << "ObserverNotifier::SetMultiDeviceSetupObserverPtr)";
    return;
  }

  cryptauth::RemoteDeviceRefList device_ref_list =
      device_sync_client_->GetSyncedDevices();

  // This condition ensures that the observer is only notified if
  // (1) it has never been notified before (i.e. timestamp is 0) and
  // (2) the user has a potential host device (i.e. IsNewUserWithPotentialHosts
  //     returns true).
  if (pref_service_->GetInt64(kNewUserPotentialHostExists) == 0 &&
      IsNewUserWithPotentialHosts(device_ref_list)) {
    observer_ptr_->OnPotentialHostExistsForNewUser();
    pref_service_->SetInt64(kNewUserPotentialHostExists,
                            clock_->Now().ToJavaTime());
  }
}

void ObserverNotifier::FlushForTesting() {
  if (observer_ptr_)
    observer_ptr_.FlushForTesting();
}

}  // namespace multidevice_setup
}  // namespace chromeos
