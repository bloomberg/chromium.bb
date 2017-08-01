// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service_regular.h"

#include <stdint.h>

#include <utility>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sys_info.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/cryptauth/chrome_cryptauth_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_proximity_auth_client.h"
#include "chrome/browser/signin/easy_unlock_notification_controller.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/cryptauth/cryptauth_access_token_fetcher.h"
#include "components/cryptauth/cryptauth_client_impl.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/cryptauth_enrollment_utils.h"
#include "components/cryptauth/cryptauth_gcm_manager_impl.h"
#include "components/cryptauth/local_device_data_provider.h"
#include "components/cryptauth/remote_device_loader.h"
#include "components/cryptauth/secure_message_delegate.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/promotion_manager.h"
#include "components/proximity_auth/proximity_auth_pref_names.h"
#include "components/proximity_auth/proximity_auth_profile_pref_manager.h"
#include "components/proximity_auth/proximity_auth_system.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/proximity_auth/switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if defined(OS_CHROMEOS)
#include "apps/app_lifetime_monitor_factory.h"
#include "ash/shell.h"
#include "base/linux_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_reauth.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/user_manager/user_manager.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#endif

namespace {

// Key name of the local device permit record dictonary in kEasyUnlockPairing.
const char kKeyPermitAccess[] = "permitAccess";

// Key name of the remote device list in kEasyUnlockPairing.
const char kKeyDevices[] = "devices";

}  // namespace

EasyUnlockServiceRegular::EasyUnlockServiceRegular(Profile* profile)
    : EasyUnlockServiceRegular(
          profile,
          EasyUnlockNotificationController::Create(profile)) {}

EasyUnlockServiceRegular::EasyUnlockServiceRegular(
    Profile* profile,
    std::unique_ptr<EasyUnlockNotificationController> notification_controller)
    : EasyUnlockService(profile),
      turn_off_flow_status_(EasyUnlockService::IDLE),
      will_unlock_using_easy_unlock_(false),
      lock_screen_last_shown_timestamp_(base::TimeTicks::Now()),
      deferring_device_load_(false),
      notification_controller_(std::move(notification_controller)),
      shown_pairing_changed_notification_(false),
      weak_ptr_factory_(this) {}

EasyUnlockServiceRegular::~EasyUnlockServiceRegular() {
}

void EasyUnlockServiceRegular::LoadRemoteDevices() {
  if (GetCryptAuthDeviceManager()->GetUnlockKeys().empty()) {
    SetProximityAuthDevices(GetAccountId(), cryptauth::RemoteDeviceList());
    return;
  }

  // This code path may be hit by:
  //   1. New devices were synced on the lock screen.
  //   2. The service was initialized while the login screen is still up.
  if (proximity_auth::ScreenlockBridge::Get()->IsLocked()) {
    PA_LOG(INFO) << "Deferring device load until screen is unlocked.";
    deferring_device_load_ = true;
    return;
  }

  remote_device_loader_.reset(new cryptauth::RemoteDeviceLoader(
      GetCryptAuthDeviceManager()->GetUnlockKeys(),
      proximity_auth_client()->GetAccountId(),
      GetCryptAuthEnrollmentManager()->GetUserPrivateKey(),
      proximity_auth_client()->CreateSecureMessageDelegate()));
  remote_device_loader_->Load(
      true /* should_load_beacon_seeds */,
      base::Bind(&EasyUnlockServiceRegular::OnRemoteDevicesLoaded,
                 weak_ptr_factory_.GetWeakPtr()));

  // Don't show promotions if EasyUnlock is already enabled.
  promotion_manager_.reset();
}

void EasyUnlockServiceRegular::OnRemoteDevicesLoaded(
    const cryptauth::RemoteDeviceList& remote_devices) {
  SetProximityAuthDevices(GetAccountId(), remote_devices);

#if defined(OS_CHROMEOS)
  // We need to store a copy of |remote devices_| in the TPM, so it can be
  // retrieved on the sign-in screen when a user session has not been started
  // yet.
  std::unique_ptr<base::ListValue> device_list(new base::ListValue());
  for (const auto& device : remote_devices) {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    std::string b64_public_key, b64_psk;
    base::Base64UrlEncode(device.public_key,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &b64_public_key);
    base::Base64UrlEncode(device.persistent_symmetric_key,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &b64_psk);

    dict->SetString("name", device.name);
    dict->SetString("psk", b64_psk);
    dict->SetString("bluetoothAddress", device.bluetooth_address);
    dict->SetString("permitId", "permit://google.com/easyunlock/v1/" +
                                    proximity_auth_client()->GetAccountId());
    dict->SetString("permitRecord.id", b64_public_key);
    dict->SetString("permitRecord.type", "license");
    dict->SetString("permitRecord.data", b64_public_key);

    std::unique_ptr<base::ListValue> beacon_seed_list(new base::ListValue());
    for (const auto& beacon_seed : device.beacon_seeds) {
      std::string b64_beacon_seed;
      base::Base64UrlEncode(beacon_seed.SerializeAsString(),
                            base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                            &b64_beacon_seed);
      beacon_seed_list->AppendString(b64_beacon_seed);
    }

    std::string serialized_beacon_seeds;
    JSONStringValueSerializer serializer(&serialized_beacon_seeds);
    serializer.Serialize(*beacon_seed_list);
    dict->SetString("serializedBeaconSeeds", serialized_beacon_seeds);

    device_list->Append(std::move(dict));
  }

  // TODO(tengs): Rename this function after the easy_unlock app is replaced.
  SetRemoteDevices(*device_list);
#endif
}

bool EasyUnlockServiceRegular::ShouldPromote() {
#if defined(OS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          proximity_auth::switches::kDisableBluetoothLowEnergyDiscovery) ||
      !base::FeatureList::IsEnabled(features::kEasyUnlockPromotions)) {
    return false;
  }

  if (!IsAllowedInternal() || IsEnabled()) {
    return false;
  }

  return true;
#else
  return false;
#endif
}

void EasyUnlockServiceRegular::StartPromotionManager() {
  if (!ShouldPromote() ||
      GetCryptAuthEnrollmentManager()->GetUserPublicKey().empty()) {
    return;
  }

  cryptauth::CryptAuthService* service =
      ChromeCryptAuthServiceFactory::GetInstance()->GetForBrowserContext(
          profile());
  local_device_data_provider_.reset(
      new cryptauth::LocalDeviceDataProvider(service));
  promotion_manager_.reset(new proximity_auth::PromotionManager(
      local_device_data_provider_.get(), notification_controller_.get(),
      pref_manager_.get(), service->CreateCryptAuthClientFactory(),
      base::MakeUnique<base::DefaultClock>(),
      base::ThreadTaskRunnerHandle::Get()));
  promotion_manager_->Start();
}

proximity_auth::ProximityAuthPrefManager*
EasyUnlockServiceRegular::GetProximityAuthPrefManager() {
  return pref_manager_.get();
}

EasyUnlockService::Type EasyUnlockServiceRegular::GetType() const {
  return EasyUnlockService::TYPE_REGULAR;
}

AccountId EasyUnlockServiceRegular::GetAccountId() const {
  const SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile());
  // |profile| has to be a signed-in profile with SigninManager already
  // created. Otherwise, just crash to collect stack.
  DCHECK(signin_manager);
  const AccountInfo account_info =
      signin_manager->GetAuthenticatedAccountInfo();
  return account_info.email.empty()
             ? EmptyAccountId()
             : AccountId::FromUserEmailGaiaId(
                   gaia::CanonicalizeEmail(account_info.email),
                   account_info.gaia);
}

void EasyUnlockServiceRegular::LaunchSetup() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(OS_CHROMEOS)
  // TODO(tengs): To keep login working for existing EasyUnlock users, we need
  // to explicitly disable login here for new users who set up EasyUnlock.
  // After a sufficient number of releases, we should make the default value
  // false.
  pref_manager_->SetIsChromeOSLoginEnabled(false);

  // Force the user to reauthenticate by showing a modal overlay (similar to the
  // lock screen). The password obtained from the reauth is cached for a short
  // period of time and used to create the cryptohome keys for sign-in.
  if (short_lived_user_context_ && short_lived_user_context_->user_context()) {
    OpenSetupApp();
  } else {
    bool reauth_success = chromeos::EasyUnlockReauth::ReauthForUserContext(
        base::Bind(&EasyUnlockServiceRegular::OpenSetupAppAfterReauth,
                   weak_ptr_factory_.GetWeakPtr()));
    if (!reauth_success)
      OpenSetupApp();
  }
#else
  OpenSetupApp();
#endif
}

#if defined(OS_CHROMEOS)
void EasyUnlockServiceRegular::HandleUserReauth(
    const chromeos::UserContext& user_context) {
  // Cache the user context for the next X minutes, so the user doesn't have to
  // reauth again.
  short_lived_user_context_.reset(new chromeos::ShortLivedUserContext(
      user_context,
      apps::AppLifetimeMonitorFactory::GetForBrowserContext(profile()),
      base::ThreadTaskRunnerHandle::Get().get()));
}

void EasyUnlockServiceRegular::OpenSetupAppAfterReauth(
    const chromeos::UserContext& user_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HandleUserReauth(user_context);

  OpenSetupApp();

  // Use this opportunity to clear the crytohome keys if it was not already
  // cleared earlier.
  const base::ListValue* devices = GetRemoteDevices();
  if (!devices || devices->empty()) {
    chromeos::EasyUnlockKeyManager* key_manager =
        chromeos::UserSessionManager::GetInstance()->GetEasyUnlockKeyManager();
    key_manager->RefreshKeys(
        user_context, base::ListValue(),
        base::Bind(&EasyUnlockServiceRegular::SetHardlockAfterKeyOperation,
                   weak_ptr_factory_.GetWeakPtr(),
                   EasyUnlockScreenlockStateHandler::NO_PAIRING));
  }
}

void EasyUnlockServiceRegular::SetHardlockAfterKeyOperation(
    EasyUnlockScreenlockStateHandler::HardlockState state_on_success,
    bool success) {
  if (success)
    SetHardlockStateForUser(GetAccountId(), state_on_success);

  // Even if the refresh keys operation suceeded, we still fetch and check the
  // cryptohome keys against the keys in local preferences as a sanity check.
  CheckCryptohomeKeysAndMaybeHardlock();
}
#endif

const base::DictionaryValue* EasyUnlockServiceRegular::GetPermitAccess() const {
  const base::DictionaryValue* pairing_dict =
      profile()->GetPrefs()->GetDictionary(prefs::kEasyUnlockPairing);
  const base::DictionaryValue* permit_dict = NULL;
  if (pairing_dict &&
      pairing_dict->GetDictionary(kKeyPermitAccess, &permit_dict))
    return permit_dict;

  return NULL;
}

void EasyUnlockServiceRegular::SetPermitAccess(
    const base::DictionaryValue& permit) {
  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  pairing_update->SetWithoutPathExpansion(
      kKeyPermitAccess, base::MakeUnique<base::Value>(permit));
}

void EasyUnlockServiceRegular::ClearPermitAccess() {
  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  pairing_update->RemoveWithoutPathExpansion(kKeyPermitAccess, NULL);
}

const base::ListValue* EasyUnlockServiceRegular::GetRemoteDevices() const {
  const base::DictionaryValue* pairing_dict =
      profile()->GetPrefs()->GetDictionary(prefs::kEasyUnlockPairing);
  const base::ListValue* devices = NULL;
  if (pairing_dict && pairing_dict->GetList(kKeyDevices, &devices))
    return devices;
  return NULL;
}

void EasyUnlockServiceRegular::SetRemoteDevices(
    const base::ListValue& devices) {
  std::string remote_devices_json;
  JSONStringValueSerializer serializer(&remote_devices_json);
  serializer.Serialize(devices);
  PA_LOG(INFO) << "Setting RemoteDevices:\n  " << remote_devices_json;

  DictionaryPrefUpdate pairing_update(profile()->GetPrefs(),
                                      prefs::kEasyUnlockPairing);
  if (devices.empty())
    pairing_update->RemoveWithoutPathExpansion(kKeyDevices, NULL);
  else
    pairing_update->SetWithoutPathExpansion(
        kKeyDevices, base::MakeUnique<base::Value>(devices));

#if defined(OS_CHROMEOS)
  // TODO(tengs): Investigate if we can determine if the remote devices were set
  // from sync or from the setup app.
  if (short_lived_user_context_ && short_lived_user_context_->user_context()) {
    // We may already have the password cached, so proceed to create the
    // cryptohome keys for sign-in or the system will be hardlocked.
    chromeos::UserSessionManager::GetInstance()
        ->GetEasyUnlockKeyManager()
        ->RefreshKeys(
            *short_lived_user_context_->user_context(), devices,
            base::Bind(&EasyUnlockServiceRegular::SetHardlockAfterKeyOperation,
                       weak_ptr_factory_.GetWeakPtr(),
                       EasyUnlockScreenlockStateHandler::NO_HARDLOCK));
  } else {
    CheckCryptohomeKeysAndMaybeHardlock();
  }
#else
  CheckCryptohomeKeysAndMaybeHardlock();
#endif
}

// This method is called from easyUnlock.setRemoteDevice JS API. It's used
// here to set the (public key, device address) pair for BLE devices.
void EasyUnlockServiceRegular::SetRemoteBleDevices(
    const base::ListValue& devices) {
  DCHECK(devices.GetSize() == 1);
  const base::DictionaryValue* dict = nullptr;
  if (devices.GetDictionary(0, &dict)) {
    std::string address, b64_public_key;
    if (dict->GetString("bluetoothAddress", &address) &&
        dict->GetString("psk", &b64_public_key)) {
      // The setup is done. Load the remote devices if the device with
      // |public_key| was already sync from CryptAuth, otherwise re-sync the
      // devices.
      if (GetCryptAuthDeviceManager()) {
        std::string public_key;
        if (!base::Base64UrlDecode(b64_public_key,
                                   base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                                   &public_key)) {
          PA_LOG(ERROR) << "Unable to base64url decode the public key: "
                        << b64_public_key;
          return;
        }
        const std::vector<cryptauth::ExternalDeviceInfo> unlock_keys =
            GetCryptAuthDeviceManager()->GetUnlockKeys();
        auto iterator = std::find_if(
            unlock_keys.begin(), unlock_keys.end(),
            [&public_key](const cryptauth::ExternalDeviceInfo& unlock_key) {
              return unlock_key.public_key() == public_key;
            });

        if (iterator != unlock_keys.end()) {
          LoadRemoteDevices();
        } else {
          GetCryptAuthDeviceManager()->ForceSyncNow(
              cryptauth::INVOCATION_REASON_FEATURE_TOGGLED);
        }
      }
    } else {
      PA_LOG(ERROR) << "Missing public key or device address";
    }
  }
}

void EasyUnlockServiceRegular::RunTurnOffFlow() {
  if (turn_off_flow_status_ == PENDING)
    return;
  DCHECK(!cryptauth_client_);

  SetTurnOffFlowStatus(PENDING);

  std::unique_ptr<cryptauth::CryptAuthClientFactory> factory =
      proximity_auth_client()->CreateCryptAuthClientFactory();
  cryptauth_client_ = factory->CreateInstance();

  cryptauth::ToggleEasyUnlockRequest request;
  request.set_enable(false);
  request.set_apply_to_all(true);
  cryptauth_client_->ToggleEasyUnlock(
      request,
      base::Bind(&EasyUnlockServiceRegular::OnToggleEasyUnlockApiComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&EasyUnlockServiceRegular::OnToggleEasyUnlockApiFailed,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EasyUnlockServiceRegular::ResetTurnOffFlow() {
  cryptauth_client_.reset();
  SetTurnOffFlowStatus(IDLE);
}

EasyUnlockService::TurnOffFlowStatus
    EasyUnlockServiceRegular::GetTurnOffFlowStatus() const {
  return turn_off_flow_status_;
}

std::string EasyUnlockServiceRegular::GetChallenge() const {
  return std::string();
}

std::string EasyUnlockServiceRegular::GetWrappedSecret() const {
  return std::string();
}

void EasyUnlockServiceRegular::RecordEasySignInOutcome(
    const AccountId& account_id,
    bool success) const {
  NOTREACHED();
}

void EasyUnlockServiceRegular::RecordPasswordLoginEvent(
    const AccountId& account_id) const {
  NOTREACHED();
}

void EasyUnlockServiceRegular::StartAutoPairing(
    const AutoPairingResultCallback& callback) {
  if (!auto_pairing_callback_.is_null()) {
    LOG(ERROR)
        << "Start auto pairing when there is another auto pairing requested.";
    callback.Run(false, std::string());
    return;
  }

  auto_pairing_callback_ = callback;

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  std::unique_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::EASY_UNLOCK_PRIVATE_ON_START_AUTO_PAIRING,
      extensions::api::easy_unlock_private::OnStartAutoPairing::kEventName,
      std::move(args)));
  extensions::EventRouter::Get(profile())->DispatchEventWithLazyListener(
      extension_misc::kEasyUnlockAppId, std::move(event));
}

void EasyUnlockServiceRegular::SetAutoPairingResult(
    bool success,
    const std::string& error) {
  DCHECK(!auto_pairing_callback_.is_null());

  auto_pairing_callback_.Run(success, error);
  auto_pairing_callback_.Reset();
}

void EasyUnlockServiceRegular::InitializeInternal() {
  proximity_auth::ScreenlockBridge::Get()->AddObserver(this);

#if defined(OS_CHROMEOS)
  // TODO(tengs): Due to badly configured browser_tests, Chrome crashes during
  // shutdown. Revisit this condition after migration is fully completed.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          proximity_auth::switches::kDisableBluetoothLowEnergyDiscovery) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType)) {
    pref_manager_.reset(new proximity_auth::ProximityAuthProfilePrefManager(
        profile()->GetPrefs()));

    // Note: There is no local state in tests.
    if (g_browser_process->local_state()) {
      pref_manager_->StartSyncingToLocalState(g_browser_process->local_state(),
                                              GetAccountId());
    }

    GetCryptAuthDeviceManager()->AddObserver(this);
    LoadRemoteDevices();
    StartPromotionManager();
  }
#endif
}

void EasyUnlockServiceRegular::ShutdownInternal() {
#if defined(OS_CHROMEOS)
  short_lived_user_context_.reset();
#endif

  turn_off_flow_status_ = EasyUnlockService::IDLE;
  proximity_auth::ScreenlockBridge::Get()->RemoveObserver(this);
}

bool EasyUnlockServiceRegular::IsAllowedInternal() const {
#if defined(OS_CHROMEOS)
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager->IsLoggedInAsUserWithGaiaAccount())
    return false;

  // TODO(tengs): Ephemeral accounts generate a new enrollment every time they
  // are added, so disable Smart Lock to reduce enrollments on server. However,
  // ephemeral accounts can be locked, so we should revisit this use case.
  if (user_manager->IsCurrentUserNonCryptohomeDataEphemeral())
    return false;

  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile()))
    return false;

  if (!profile()->GetPrefs()->GetBoolean(prefs::kEasyUnlockAllowed))
    return false;

  return true;
#else
  // TODO(xiyuan): Revisit when non-chromeos platforms are supported.
  return false;
#endif
}

void EasyUnlockServiceRegular::OnWillFinalizeUnlock(bool success) {
  will_unlock_using_easy_unlock_ = success;
}

void EasyUnlockServiceRegular::OnSuspendDoneInternal() {
  lock_screen_last_shown_timestamp_ = base::TimeTicks::Now();
}

void EasyUnlockServiceRegular::OnSyncStarted() {
  unlock_keys_before_sync_ = GetCryptAuthDeviceManager()->GetUnlockKeys();
}

void EasyUnlockServiceRegular::OnSyncFinished(
    cryptauth::CryptAuthDeviceManager::SyncResult sync_result,
    cryptauth::CryptAuthDeviceManager::DeviceChangeResult
        device_change_result) {
  if (sync_result == cryptauth::CryptAuthDeviceManager::SyncResult::FAILURE)
    return;

  std::set<std::string> public_keys_before_sync;
  for (const auto& device_info : unlock_keys_before_sync_) {
    public_keys_before_sync.insert(device_info.public_key());
  }
  unlock_keys_before_sync_.clear();

  std::vector<cryptauth::ExternalDeviceInfo> unlock_keys_after_sync =
      GetCryptAuthDeviceManager()->GetUnlockKeys();
  std::set<std::string> public_keys_after_sync;
  for (const auto& device_info : unlock_keys_after_sync) {
    public_keys_after_sync.insert(device_info.public_key());
  }

  if (public_keys_before_sync == public_keys_after_sync)
    return;

  // Show the appropriate notification if an unlock key is first synced or if it
  // changes an existing key.
  // Note: We do not show a notification when EasyUnlock is disabled by sync.
  if (public_keys_after_sync.size() > 0) {
    if (public_keys_before_sync.size() == 0) {
      notification_controller_->ShowChromebookAddedNotification();
    } else {
      shown_pairing_changed_notification_ = true;
      notification_controller_->ShowPairingChangeNotification();
    }
  }

  // The enrollment has finished when the sync is finished.
  StartPromotionManager();

  LoadRemoteDevices();
}

void EasyUnlockServiceRegular::OnScreenDidLock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  will_unlock_using_easy_unlock_ = false;
  lock_screen_last_shown_timestamp_ = base::TimeTicks::Now();
}

void EasyUnlockServiceRegular::OnScreenDidUnlock(
    proximity_auth::ScreenlockBridge::LockHandler::ScreenType screen_type) {
  bool is_lock_screen =
      screen_type == proximity_auth::ScreenlockBridge::LockHandler::LOCK_SCREEN;

  if (!will_unlock_using_easy_unlock_ && pref_manager_ &&
      (is_lock_screen || !base::CommandLine::ForCurrentProcess()->HasSwitch(
                             proximity_auth::switches::kEnableChromeOSLogin))) {
    // If a password was used, then record the current timestamp. This timestamp
    // is used to enforce password reauths after a certain time has elapsed.
    pref_manager_->SetLastPasswordEntryTimestampMs(
        base::Time::Now().ToJavaTime());
  }

  // If we tried to load remote devices (e.g. after a sync) while the screen was
  // locked, we can now load the new remote devices.
  // Note: This codepath may be reachable when the login screen unlocks.
  if (deferring_device_load_) {
    PA_LOG(INFO) << "Loading deferred devices after screen unlock.";
    deferring_device_load_ = false;
    LoadRemoteDevices();
  }

  if (shown_pairing_changed_notification_) {
    shown_pairing_changed_notification_ = false;
    std::vector<cryptauth::ExternalDeviceInfo> unlock_keys =
        GetCryptAuthDeviceManager()->GetUnlockKeys();
    if (!unlock_keys.empty()) {
      // TODO(tengs): Right now, we assume that there is only one possible
      // unlock key. We need to update this notification be more generic.
      notification_controller_->ShowPairingChangeAppliedNotification(
          unlock_keys[0].friendly_device_name());
    }
  }

  // Do not process events for the login screen.
  if (!is_lock_screen)
    return;

  // Only record metrics for users who have enabled the feature.
  if (IsEnabled()) {
    EasyUnlockAuthEvent event = will_unlock_using_easy_unlock_
                                    ? EASY_UNLOCK_SUCCESS
                                    : GetPasswordAuthEvent();
    RecordEasyUnlockScreenUnlockEvent(event);

    if (will_unlock_using_easy_unlock_) {
      RecordEasyUnlockScreenUnlockDuration(base::TimeTicks::Now() -
                                           lock_screen_last_shown_timestamp_);
    }
  }

  will_unlock_using_easy_unlock_ = false;
}

void EasyUnlockServiceRegular::OnFocusedUserChanged(
    const AccountId& account_id) {
  // Nothing to do.
}

void EasyUnlockServiceRegular::SetTurnOffFlowStatus(TurnOffFlowStatus status) {
  turn_off_flow_status_ = status;
  NotifyTurnOffOperationStatusChanged();
}

void EasyUnlockServiceRegular::OnToggleEasyUnlockApiComplete(
    const cryptauth::ToggleEasyUnlockResponse& response) {
  cryptauth_client_.reset();

  GetCryptAuthDeviceManager()->ForceSyncNow(
      cryptauth::InvocationReason::INVOCATION_REASON_FEATURE_TOGGLED);
  EasyUnlockService::ResetLocalStateForUser(GetAccountId());
  SetRemoteDevices(base::ListValue());
  SetTurnOffFlowStatus(IDLE);
  ReloadAppAndLockScreen();
}

void EasyUnlockServiceRegular::OnToggleEasyUnlockApiFailed(
    const std::string& error_message) {
  LOG(WARNING) << "Failed to turn off Smart Lock: " << error_message;
  SetTurnOffFlowStatus(FAIL);
}

cryptauth::CryptAuthEnrollmentManager*
EasyUnlockServiceRegular::GetCryptAuthEnrollmentManager() {
  cryptauth::CryptAuthEnrollmentManager* manager =
      ChromeCryptAuthServiceFactory::GetInstance()
          ->GetForBrowserContext(profile())
          ->GetCryptAuthEnrollmentManager();
  DCHECK(manager);
  return manager;
}

cryptauth::CryptAuthDeviceManager*
EasyUnlockServiceRegular::GetCryptAuthDeviceManager() {
  cryptauth::CryptAuthDeviceManager* manager =
      ChromeCryptAuthServiceFactory::GetInstance()
          ->GetForBrowserContext(profile())
          ->GetCryptAuthDeviceManager();
  DCHECK(manager);
  return manager;
}
