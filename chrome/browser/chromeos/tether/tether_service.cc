// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tether/tether_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
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

namespace {

// TODO (hansberry): Experiment with intervals to determine ideal advertising
//                   interval parameters. See crbug.com/753215.
constexpr int64_t kMinAdvertisingIntervalMilliseconds = 100;
constexpr int64_t kMaxAdvertisingIntervalMilliseconds = 100;

}  // namespace

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

  // If we initially assume that BLE advertising is not supported, it will
  // result in Tether's Settings and Quick Settings sections not being visible
  // when the user logs in with Bluetooth disabled (because the TechnologyState
  // will be UNAVAILABLE, instead of the desired UNINITIALIZED).
  //
  // Initially assuming that BLE advertising *is* supported works well for most
  // devices, but if a user first logs into a device without BLE advertising
  // support and with Bluetooth disabled, Tether will be visible in Settings and
  // Quick Settings, but disappear upon enabling Bluetooth. This is an
  // acceptable edge case, and likely rare because Bluetooth is enabled by
  // default on new logins. Additionally, through this pref, we will record if
  // BLE advertising is not supported and remember that for future logins.
  registry->RegisterBooleanPref(prefs::kInstantTetheringBleAdvertisingSupported,
                                true);

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
    chromeos::NetworkConnectionHandler* network_connection_handler,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  chromeos::tether::Initializer::Init(
      cryptauth_service, std::move(notification_presenter), pref_service,
      token_service, network_state_handler,
      managed_network_configuration_handler, network_connect,
      network_connection_handler, adapter);
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
      chromeos::NetworkHandler::Get()->network_connection_handler(), adapter_);
}

void TetherService::StopTetherIfNecessary() {
  initializer_delegate_->ShutdownTether();
}

void TetherService::Shutdown() {
  if (shut_down_)
    return;

  // Record to UMA Tether's last feature state before it is shutdown.
  // Tether's feature state at the end of its life is the most likely to
  // accurately represent how it has been used by the user.
  RecordTetherFeatureState();

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
  // Once the BLE advertising interval has been set (regardless of if BLE
  // advertising is supported), simply update the TechnologyState.
  if (has_attempted_to_set_ble_advertising_interval_) {
    UpdateTetherTechnologyState();
    return;
  }

  // If the BluetoothAdapter was not powered when first fetched (see
  // OnBluetoothAdapterFetched()), now attempt to set the BLE advertising
  // interval.
  if (powered)
    SetBleAdvertisingInterval();
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
  if (!adapter_)
    return;

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
  switch (GetTetherFeatureState()) {
    case OTHER_OR_UNKNOWN:
    case BLE_ADVERTISING_NOT_SUPPORTED:
    case SCREEN_LOCKED:
    case NO_AVAILABLE_HOSTS:
    case CELLULAR_DISABLED:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_UNAVAILABLE;

    case PROHIBITED:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_PROHIBITED;

    case BLUETOOTH_DISABLED:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_UNINITIALIZED;

    case USER_PREFERENCE_DISABLED:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_AVAILABLE;

    case ENABLED:
      return chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED;

    default:
      return chromeos::NetworkStateHandler::TechnologyState::
          TECHNOLOGY_UNAVAILABLE;
  }
}

void TetherService::OnBluetoothAdapterFetched(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (shut_down_)
    return;

  adapter_ = adapter;
  adapter_->AddObserver(this);

  // Update TechnologyState in case Tether is otherwise available but Bluetooth
  // is off.
  UpdateTetherTechnologyState();

  // If |adapter_| is not powered, wait until it is to call
  // SetBleAdvertisingInterval(). See AdapterPoweredChanged().
  if (IsBluetoothAvailable())
    SetBleAdvertisingInterval();

  // The user has just logged in; display the "enable Bluetooth" notification if
  // applicable.
  if (CanEnableBluetoothNotificationBeShown())
    notification_presenter_->NotifyEnableBluetooth();
}

void TetherService::OnBluetoothAdapterAdvertisingIntervalSet() {
  has_attempted_to_set_ble_advertising_interval_ = true;
  SetIsBleAdvertisingSupportedPref(true);

  UpdateTetherTechnologyState();
}

void TetherService::OnBluetoothAdapterAdvertisingIntervalError(
    device::BluetoothAdvertisement::ErrorCode status) {
  has_attempted_to_set_ble_advertising_interval_ = true;
  SetIsBleAdvertisingSupportedPref(false);

  UpdateTetherTechnologyState();
}

void TetherService::SetBleAdvertisingInterval() {
  DCHECK(IsBluetoothAvailable());
  adapter_->SetAdvertisingInterval(
      base::TimeDelta::FromMilliseconds(kMinAdvertisingIntervalMilliseconds),
      base::TimeDelta::FromMilliseconds(kMaxAdvertisingIntervalMilliseconds),
      base::Bind(&TetherService::OnBluetoothAdapterAdvertisingIntervalSet,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&TetherService::OnBluetoothAdapterAdvertisingIntervalError,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool TetherService::GetIsBleAdvertisingSupportedPref() {
  return profile_->GetPrefs()->GetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported);
}

void TetherService::SetIsBleAdvertisingSupportedPref(
    bool is_ble_advertising_supported) {
  profile_->GetPrefs()->SetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported,
      is_ble_advertising_supported);
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

TetherService::TetherFeatureState TetherService::GetTetherFeatureState() {
  if (shut_down_ || suspended_)
    return OTHER_OR_UNKNOWN;

  if (!GetIsBleAdvertisingSupportedPref())
    return BLE_ADVERTISING_NOT_SUPPORTED;

  if (session_manager_client_->IsScreenLocked())
    return SCREEN_LOCKED;

  if (!HasSyncedTetherHosts())
    return NO_AVAILABLE_HOSTS;

  // If Cellular technology is available, then Tether technology is treated
  // as a subset of Cellular, and it should only be enabled when Cellular
  // technology is enabled.
  if (IsCellularAvailableButNotEnabled())
    return CELLULAR_DISABLED;

  if (!IsAllowedByPolicy())
    return PROHIBITED;

  // TODO (hansberry): When !IsBluetoothAvailable(), this results in a weird
  // UI state for Settings where the toggle is clickable but immediately
  // becomes disabled after enabling it. See crbug.com/753195.
  if (!IsBluetoothAvailable())
    return BLUETOOTH_DISABLED;

  if (!IsEnabledbyPreference())
    return USER_PREFERENCE_DISABLED;

  return ENABLED;
}

void TetherService::RecordTetherFeatureState() {
  TetherFeatureState tether_feature_state = GetTetherFeatureState();
  DCHECK(tether_feature_state != TetherFeatureState::TETHER_FEATURE_STATE_MAX);
  UMA_HISTOGRAM_ENUMERATION("InstantTethering.FinalFeatureState",
                            tether_feature_state,
                            TetherFeatureState::TETHER_FEATURE_STATE_MAX);
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
