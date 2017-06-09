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

void TetherService::StartTetherIfEnabled() {
  if (GetTetherTechnologyState() !=
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    return;
  }

  auto notification_presenter =
      base::MakeUnique<chromeos::tether::TetherNotificationPresenter>(
          profile_, message_center::MessageCenter::Get(),
          chromeos::NetworkConnect::Get());
  chromeos::tether::Initializer::Init(
      cryptauth_service_, std::move(notification_presenter),
      profile_->GetPrefs(),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
      network_state_handler_,
      chromeos::NetworkHandler::Get()->managed_network_configuration_handler(),
      chromeos::NetworkConnect::Get(),
      chromeos::NetworkHandler::Get()->network_connection_handler());
}

void TetherService::StopTether() {
  chromeos::tether::Initializer::Shutdown();
}

void TetherService::Shutdown() {
  if (shut_down_)
    return;

  shut_down_ = true;

  power_manager_client_->RemoveObserver(this);
  session_manager_client_->RemoveObserver(this);
  cryptauth_service_->GetCryptAuthDeviceManager()->RemoveObserver(this);
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (adapter_)
    adapter_->RemoveObserver(this);
  registrar_.RemoveAll();

  UpdateTetherTechnologyState();
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
  chromeos::NetworkStateHandler::TechnologyState tether_technology_state =
      GetTetherTechnologyState();

  network_state_handler_->SetTetherTechnologyState(tether_technology_state);

  if (tether_technology_state ==
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    StartTetherIfEnabled();
  } else {
    StopTether();
  }
}

chromeos::NetworkStateHandler::TechnologyState
TetherService::GetTetherTechnologyState() {
  if (shut_down_ || suspended_ || session_manager_client_->IsScreenLocked() ||
      !HasSyncedTetherHosts()) {
    return chromeos::NetworkStateHandler::TechnologyState::
        TECHNOLOGY_UNAVAILABLE;
  } else if (!IsAllowedByPolicy()) {
    return chromeos::NetworkStateHandler::TechnologyState::
        TECHNOLOGY_PROHIBITED;
  } else if (!IsBluetoothAvailable() || IsCellularAvailableButNotEnabled()) {
    // If Cellular technology is available, then Tether technology is treated
    // as a subset of Cellular, and it should only be enabled when Cellular
    // technology is enabled.
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
