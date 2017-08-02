// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tether/tether_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/net/tether_notification_presenter.h"
#include "chrome/browser/chromeos/tether/tether_service_factory.h"
#include "chrome/browser/cryptauth/chrome_cryptauth_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/components/tether/initializer.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/cryptauth/cryptauth_service.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "ui/message_center/message_center.h"

// static
TetherService* TetherService::Get(Profile* profile) {
  if (IsFeatureFlagEnabled())
    return TetherServiceFactory::GetForBrowserContext(profile);

  return nullptr;
}

// static
void TetherService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kInstantTetheringAllowed, true);
  registry->RegisterBooleanPref(prefs::kInstantTetheringEnabled, true);
  chromeos::tether::Initializer::RegisterProfilePrefs(registry);
}

// static
bool TetherService::IsFeatureFlagEnabled() {
  return base::FeatureList::IsEnabled(features::kInstantTethering);
}

void TetherService::InitializerDelegate::InitializeTether(
    cryptauth::CryptAuthService* cryptauth_service,
    chromeos::tether::NotificationPresenter* notification_presenter,
    PrefService* pref_service,
    ProfileOAuth2TokenService* token_service,
    chromeos::NetworkStateHandler* network_state_handler,
    chromeos::ManagedNetworkConfigurationHandler*
        managed_network_configuration_handler,
    chromeos::NetworkConnect* network_connect,
    chromeos::NetworkConnectionHandler* network_connection_handler) {
  chromeos::tether::Initializer::Init(
      cryptauth_service, std::move(notification_presenter), pref_service,
      token_service, network_state_handler,
      managed_network_configuration_handler, network_connect,
      network_connection_handler);
}

void TetherService::InitializerDelegate::ShutdownTether() {
  chromeos::tether::Initializer::Shutdown();
}

TetherService::TetherService(
    Profile* profile,
    chromeos::PowerManagerClient* power_manager_client,
    chromeos::SessionManagerClient* session_manager_client,
    cryptauth::CryptAuthService* cryptauth_service,
    chromeos::NetworkStateHandler* network_state_handler)
    : profile_(profile),
      power_manager_client_(power_manager_client),
      session_manager_client_(session_manager_client),
      cryptauth_service_(cryptauth_service),
      network_state_handler_(network_state_handler),
      initializer_delegate_(base::MakeUnique<InitializerDelegate>()),
      notification_presenter_(
          base::MakeUnique<chromeos::tether::TetherNotificationPresenter>(
              profile_,
              message_center::MessageCenter::Get(),
              chromeos::NetworkConnect::Get())),
      weak_ptr_factory_(this) {
  power_manager_client_->AddObserver(this);
  session_manager_client_->AddObserver(this);

  cryptauth_service_->GetCryptAuthDeviceManager()->AddObserver(this);

  network_state_handler_->AddObserver(this, FROM_HERE);

  registrar_.Init(profile_->GetPrefs());
  registrar_.Add(prefs::kInstantTetheringAllowed,
                 base::Bind(&TetherService::OnPrefsChanged,
                            weak_ptr_factory_.GetWeakPtr()));

  // GetAdapter may call OnBluetoothAdapterFetched immediately which can cause
  // problems with the Fake implementation since the class is not fully
  // constructed yet. Post the GetAdapter call to avoid this.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(device::BluetoothAdapterFactory::GetAdapter,
                 base::Bind(&TetherService::OnBluetoothAdapterFetched,
                            weak_ptr_factory_.GetWeakPtr())));
}

TetherService::~TetherService() {}

void TetherService::StartTetherIfPossible() {
  if (GetTetherTechnologyState() !=
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    return;
  }

  initializer_delegate_->InitializeTether(
      cryptauth_service_, notification_presenter_.get(), profile_->GetPrefs(),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
      network_state_handler_,
      chromeos::NetworkHandler::Get()->managed_network_configuration_handler(),
      chromeos::NetworkConnect::Get(),
      chromeos::NetworkHandler::Get()->network_connection_handler());
}

void TetherService::StopTetherIfNecessary() {
  initializer_delegate_->ShutdownTether();
}

void TetherService::Shutdown() {
  if (shut_down_)
    return;

  shut_down_ = true;

  // Remove all observers. This ensures that once Shutdown() is called, no more
  // calls to UpdateTetherTechnologyState() will be triggered.
  power_manager_client_->RemoveObserver(this);
  session_manager_client_->RemoveObserver(this);
  cryptauth_service_->GetCryptAuthDeviceManager()->RemoveObserver(this);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (adapter_)
    adapter_->RemoveObserver(this);
  registrar_.RemoveAll();

  // Shut down the feature. Note that this does not change Tether's technology
  // state in NetworkStateHandler because doing so could cause visual jank just
  // as the user logs out.
  StopTetherIfNecessary();

  notification_presenter_.reset();
}

void TetherService::SuspendImminent() {
  suspended_ = true;
  UpdateTetherTechnologyState();
}

void TetherService::SuspendDone(const base::TimeDelta& sleep_duration) {
  suspended_ = false;
  UpdateTetherTechnologyState();
}

void TetherService::ScreenIsLocked() {
  UpdateTetherTechnologyState();
}

void TetherService::ScreenIsUnlocked() {
  UpdateTetherTechnologyState();
}

void TetherService::OnSyncFinished(
    cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
    cryptauth::CryptAuthDeviceManager::DeviceChangeResult
        device_change_result) {
  if (device_change_result ==
      cryptauth::CryptAuthDeviceManager::DeviceChangeResult::UNCHANGED) {
    return;
  }

  UpdateTetherTechnologyState();
}

void TetherService::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                          bool powered) {
  UpdateTetherTechnologyState();
}

void TetherService::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  if (CanEnableBluetoothNotificationBeShown()) {
    // If the device has just been disconnected from the Internet, the user may
    // be looking for a way to find a connection. If Bluetooth is disabled and
    // is preventing Tether connections from being found, alert the user.
    notification_presenter_->NotifyEnableBluetooth();
  } else {
    notification_presenter_->RemoveEnableBluetoothNotification();
  }
}

void TetherService::DeviceListChanged() {
  bool was_pref_enabled = IsEnabledbyPreference();
  chromeos::NetworkStateHandler::TechnologyState tether_technology_state =
      network_state_handler_->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether());

  // If |was_tether_setting_enabled| differs from the new Tether
  // TechnologyState, the settings toggle has been changed. Update the
  // kInstantTetheringEnabled user pref accordingly.
  bool is_enabled;
  if (was_pref_enabled && tether_technology_state ==
                              chromeos::NetworkStateHandler::TechnologyState::
                                  TECHNOLOGY_AVAILABLE) {
    is_enabled = false;
  } else if (!was_pref_enabled && tether_technology_state ==
                                      chromeos::NetworkStateHandler::
                                          TechnologyState::TECHNOLOGY_ENABLED) {
    is_enabled = true;
  } else {
    is_enabled = was_pref_enabled;
  }

  if (is_enabled != was_pref_enabled) {
    profile_->GetPrefs()->SetBoolean(prefs::kInstantTetheringEnabled,
                                     is_enabled);
  }
  UpdateTetherTechnologyState();
}

void TetherService::OnPrefsChanged() {
  UpdateTetherTechnologyState();
}

bool TetherService::HasSyncedTetherHosts() const {
  return !cryptauth_service_->GetCryptAuthDeviceManager()
              ->GetTetherHosts()
              .empty();
}

void TetherService::UpdateTetherTechnologyState() {
  chromeos::NetworkStateHandler::TechnologyState new_tether_technology_state =
      GetTetherTechnologyState();

  if (new_tether_technology_state ==
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    // If Tether should be enabled, notify NetworkStateHandler before starting
    // up the component. This ensures that it is not possible to add Tether
    // networks before the network stack is ready for them.
    network_state_handler_->SetTetherTechnologyState(
        new_tether_technology_state);
    StartTetherIfPossible();
  } else {
    // If Tether should not be enabled, shut down the component before notifying
    // NetworkStateHandler. This ensures that nothing in the Tether component
    // attempts to edit Tether networks or properties when the network stack is
    // not ready for them.
    StopTetherIfNecessary();
    network_state_handler_->SetTetherTechnologyState(
        new_tether_technology_state);
  }

  if (!CanEnableBluetoothNotificationBeShown())
    notification_presenter_->RemoveEnableBluetoothNotification();
}

chromeos::NetworkStateHandler::TechnologyState
TetherService::GetTetherTechnologyState() {
  if (shut_down_ || suspended_ || session_manager_client_->IsScreenLocked() ||
      !HasSyncedTetherHosts() || IsCellularAvailableButNotEnabled()) {
    // If Cellular technology is available, then Tether technology is treated
    // as a subset of Cellular, and it should only be enabled when Cellular
    // technology is enabled.
    return chromeos::NetworkStateHandler::TechnologyState::
        TECHNOLOGY_UNAVAILABLE;
  } else if (!IsAllowedByPolicy()) {
    return chromeos::NetworkStateHandler::TechnologyState::
        TECHNOLOGY_PROHIBITED;
  } else if (!IsBluetoothAvailable()) {
    // TODO (hansberry): When !IsBluetoothAvailable(), this results in a weird
    // UI state for Settings where the toggle is clickable but immediately
    // becomes disabled after enabling it.  Possible solution: grey out the
    // toggle and tell the user to turn Bluetooth on?
    return chromeos::NetworkStateHandler::TechnologyState::
        TECHNOLOGY_UNINITIALIZED;
  } else if (!IsEnabledbyPreference()) {
    return chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE;
  }

  return chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED;
}

void TetherService::OnBluetoothAdapterFetched(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (shut_down_)
    return;

  adapter_ = adapter;
  adapter_->AddObserver(this);

  UpdateTetherTechnologyState();

  // The user has just logged in; display the "enable Bluetooth" notification if
  // applicable.
  if (CanEnableBluetoothNotificationBeShown())
    notification_presenter_->NotifyEnableBluetooth();
}

bool TetherService::IsBluetoothAvailable() const {
  return adapter_.get() && adapter_->IsPresent() && adapter_->IsPowered();
}

bool TetherService::IsCellularAvailableButNotEnabled() const {
  return (network_state_handler_->IsTechnologyAvailable(
              chromeos::NetworkTypePattern::Cellular()) &&
          !network_state_handler_->IsTechnologyEnabled(
              chromeos::NetworkTypePattern::Cellular()));
}

bool TetherService::IsAllowedByPolicy() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kInstantTetheringAllowed);
}

bool TetherService::IsEnabledbyPreference() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kInstantTetheringEnabled);
}

bool TetherService::CanEnableBluetoothNotificationBeShown() {
  if (!IsEnabledbyPreference() || IsBluetoothAvailable() ||
      GetTetherTechnologyState() !=
          chromeos::NetworkStateHandler::TechnologyState::
              TECHNOLOGY_UNINITIALIZED) {
    // Cannot be shown unless Tether is uninitialized.
    return false;
  }

  const chromeos::NetworkState* network =
      network_state_handler_->DefaultNetwork();
  if (network &&
      (network->IsConnectingState() || network->IsConnectedState())) {
    // If an Internet connection is available, there is no need to show a
    // notification which helps the user find an Internet connection.
    return false;
  }

  return true;
}

void TetherService::SetInitializerDelegateForTest(
    std::unique_ptr<InitializerDelegate> initializer_delegate) {
  initializer_delegate_ = std::move(initializer_delegate);
}

void TetherService::SetNotificationPresenterForTest(
    std::unique_ptr<chromeos::tether::NotificationPresenter>
        notification_presenter) {
  notification_presenter_ = std::move(notification_presenter);
}
