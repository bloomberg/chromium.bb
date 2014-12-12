// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/easy_unlock_auth_attempt.h"
#include "chrome/browser/signin/easy_unlock_service_factory.h"
#include "chrome/browser/signin/easy_unlock_service_observer.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/easy_unlock_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/proximity_auth/switches.h"
#include "components/user_manager/user.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"
#include "grit/browser_resources.h"

#if defined(OS_CHROMEOS)
#include "base/sys_info.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_key_manager.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#endif

namespace {

extensions::ComponentLoader* GetComponentLoader(
    content::BrowserContext* context) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(context);
  ExtensionService* extension_service = extension_system->extension_service();
  return extension_service->component_loader();
}

PrefService* GetLocalState() {
  return g_browser_process ? g_browser_process->local_state() : NULL;
}

}  // namespace

// static
EasyUnlockService* EasyUnlockService::Get(Profile* profile) {
  return EasyUnlockServiceFactory::GetForProfile(profile);
}

// static
EasyUnlockService* EasyUnlockService::GetForUser(
    const user_manager::User& user) {
#if defined(OS_CHROMEOS)
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(&user);
  if (!profile)
    return NULL;
  return EasyUnlockService::Get(profile);
#else
  return NULL;
#endif
}

// static
bool EasyUnlockService::IsSignInEnabled() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      proximity_auth::switches::kDisableEasyUnlock);
}

class EasyUnlockService::BluetoothDetector
    : public device::BluetoothAdapter::Observer {
 public:
  explicit BluetoothDetector(EasyUnlockService* service)
      : service_(service),
        weak_ptr_factory_(this) {
  }

  ~BluetoothDetector() override {
    if (adapter_.get())
      adapter_->RemoveObserver(this);
  }

  void Initialize() {
    if (!device::BluetoothAdapterFactory::IsBluetoothAdapterAvailable())
      return;

    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothDetector::OnAdapterInitialized,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  bool IsPresent() const { return adapter_.get() && adapter_->IsPresent(); }

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override {
    service_->OnBluetoothAdapterPresentChanged();
  }

 private:
  void OnAdapterInitialized(scoped_refptr<device::BluetoothAdapter> adapter) {
    adapter_ = adapter;
    adapter_->AddObserver(this);
    service_->OnBluetoothAdapterPresentChanged();
  }

  // Owner of this class and should out-live this class.
  EasyUnlockService* service_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::WeakPtrFactory<BluetoothDetector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDetector);
};

#if defined(OS_CHROMEOS)
class EasyUnlockService::PowerMonitor
    : public chromeos::PowerManagerClient::Observer {
 public:
  explicit PowerMonitor(EasyUnlockService* service)
      : service_(service),
        waking_up_(false),
        weak_ptr_factory_(this) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        AddObserver(this);
  }

  virtual ~PowerMonitor() {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        RemoveObserver(this);
  }

  bool waking_up() const { return waking_up_; }

 private:
  // chromeos::PowerManagerClient::Observer:
  virtual void SuspendImminent() override {
    service_->PrepareForSuspend();
  }

  virtual void SuspendDone(const base::TimeDelta& sleep_duration) override {
    waking_up_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&PowerMonitor::ResetWakingUp,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(5));
    service_->UpdateAppState();
    // Note that |this| may get deleted after |UpdateAppState| is called.
  }

  void ResetWakingUp() {
    waking_up_ = false;
    service_->UpdateAppState();
  }

  EasyUnlockService* service_;
  bool waking_up_;
  base::WeakPtrFactory<PowerMonitor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PowerMonitor);
};
#endif

EasyUnlockService::EasyUnlockService(Profile* profile)
    : profile_(profile),
      bluetooth_detector_(new BluetoothDetector(this)),
      shut_down_(false),
      weak_ptr_factory_(this) {
  extensions::ExtensionSystem::Get(profile_)->ready().Post(
      FROM_HERE,
      base::Bind(&EasyUnlockService::Initialize,
                 weak_ptr_factory_.GetWeakPtr()));
}

EasyUnlockService::~EasyUnlockService() {
}

// static
void EasyUnlockService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kEasyUnlockAllowed,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEasyUnlockEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kEasyUnlockPairing,
      new base::DictionaryValue(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEasyUnlockProximityRequired,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
void EasyUnlockService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kEasyUnlockHardlockState);
}

// static
void EasyUnlockService::ResetLocalStateForUser(const std::string& user_id) {
  DCHECK(!user_id.empty());

  PrefService* local_state = GetLocalState();
  if (!local_state)
    return;

  DictionaryPrefUpdate update(local_state, prefs::kEasyUnlockHardlockState);
  update->RemoveWithoutPathExpansion(user_id, NULL);
}

bool EasyUnlockService::IsAllowed() {
  if (shut_down_)
    return false;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          proximity_auth::switches::kDisableEasyUnlock)) {
    return false;
  }

  if (!IsAllowedInternal())
    return false;

#if defined(OS_CHROMEOS)
  if (!bluetooth_detector_->IsPresent())
    return false;

  return true;
#else
  // TODO(xiyuan): Revisit when non-chromeos platforms are supported.
  return false;
#endif
}

void EasyUnlockService::SetHardlockState(
    EasyUnlockScreenlockStateHandler::HardlockState state) {
  const std::string user_id = GetUserEmail();
  if (user_id.empty())
    return;

  SetHardlockStateForUser(user_id, state);
}

EasyUnlockScreenlockStateHandler::HardlockState
EasyUnlockService::GetHardlockState() const {
  EasyUnlockScreenlockStateHandler::HardlockState state;
  if (GetPersistedHardlockState(&state))
    return state;

  return EasyUnlockScreenlockStateHandler::NO_HARDLOCK;
}

bool EasyUnlockService::GetPersistedHardlockState(
    EasyUnlockScreenlockStateHandler::HardlockState* state) const {
  std::string user_id = GetUserEmail();
  if (user_id.empty())
    return false;

  PrefService* local_state = GetLocalState();
  if (!local_state)
    return false;

  const base::DictionaryValue* dict =
      local_state->GetDictionary(prefs::kEasyUnlockHardlockState);
  int state_int;
  if (dict && dict->GetIntegerWithoutPathExpansion(user_id, &state_int)) {
    *state =
        static_cast<EasyUnlockScreenlockStateHandler::HardlockState>(state_int);
    return true;
  }

  return false;
}

void EasyUnlockService::ShowInitialUserState() {
  if (!GetScreenlockStateHandler())
    return;

  EasyUnlockScreenlockStateHandler::HardlockState state;
  bool has_persisted_state = GetPersistedHardlockState(&state);
  if (!has_persisted_state)
    return;

  if (state == EasyUnlockScreenlockStateHandler::NO_HARDLOCK) {
    // Show connecting icon early when there is a persisted non hardlock state.
    UpdateScreenlockState(
        EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING);
  } else {
    screenlock_state_handler_->MaybeShowHardlockUI();
  }
}

EasyUnlockScreenlockStateHandler*
    EasyUnlockService::GetScreenlockStateHandler() {
  if (!IsAllowed())
    return NULL;
  if (!screenlock_state_handler_) {
    screenlock_state_handler_.reset(new EasyUnlockScreenlockStateHandler(
        GetUserEmail(),
        GetHardlockState(),
        ScreenlockBridge::Get()));
  }
  return screenlock_state_handler_.get();
}

bool EasyUnlockService::UpdateScreenlockState(
    EasyUnlockScreenlockStateHandler::State state) {
  EasyUnlockScreenlockStateHandler* handler = GetScreenlockStateHandler();
  if (!handler)
    return false;

  handler->ChangeState(state);

  if (state != EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED &&
      auth_attempt_.get()) {
    auth_attempt_.reset();

    if (!handler->InStateValidOnRemoteAuthFailure())
      HandleAuthFailure(GetUserEmail());
  }
  return true;
}

void EasyUnlockService::AttemptAuth(const std::string& user_id) {
  auth_attempt_.reset(new EasyUnlockAuthAttempt(
      profile_,
      GetUserEmail(),
      GetType() == TYPE_REGULAR ? EasyUnlockAuthAttempt::TYPE_UNLOCK
                                : EasyUnlockAuthAttempt::TYPE_SIGNIN));
  if (!auth_attempt_->Start(user_id))
    auth_attempt_.reset();
}

void EasyUnlockService::FinalizeUnlock(bool success) {
  if (!auth_attempt_.get())
    return;

  auth_attempt_->FinalizeUnlock(GetUserEmail(), success);
  auth_attempt_.reset();

  // Make sure that the lock screen is updated on failure.
  if (!success)
    HandleAuthFailure(GetUserEmail());
}

void EasyUnlockService::FinalizeSignin(const std::string& key) {
  if (!auth_attempt_.get())
    return;
  std::string wrapped_secret = GetWrappedSecret();
  if (!wrapped_secret.empty())
    auth_attempt_->FinalizeSignin(GetUserEmail(), wrapped_secret, key);
  auth_attempt_.reset();

  // Processing empty key is equivalent to auth cancellation. In this case the
  // signin request will not actually be processed by login stack, so the lock
  // screen state should be set from here.
  if (key.empty())
    HandleAuthFailure(GetUserEmail());
}

void EasyUnlockService::HandleAuthFailure(const std::string& user_id) {
  if (user_id != GetUserEmail())
    return;

  if (!screenlock_state_handler_.get())
    return;

  screenlock_state_handler_->SetHardlockState(
      EasyUnlockScreenlockStateHandler::LOGIN_FAILED);
}

void EasyUnlockService::CheckCryptohomeKeysAndMaybeHardlock() {
#if defined(OS_CHROMEOS)
  std::string user_id = GetUserEmail();
  if (user_id.empty())
    return;

  const base::ListValue* device_list = GetRemoteDevices();
  std::set<std::string> paired_devices;
  if (device_list) {
    chromeos::EasyUnlockDeviceKeyDataList parsed_paired;
    chromeos::EasyUnlockKeyManager::RemoteDeviceListToDeviceDataList(
        *device_list, &parsed_paired);
    for (const auto& device_key_data : parsed_paired)
      paired_devices.insert(device_key_data.psk);
  }
  if (paired_devices.empty()) {
    SetHardlockState(EasyUnlockScreenlockStateHandler::NO_PAIRING);
    return;
  }

  // No need to compare if a change is already recorded.
  if (GetHardlockState() == EasyUnlockScreenlockStateHandler::PAIRING_CHANGED ||
      GetHardlockState() == EasyUnlockScreenlockStateHandler::PAIRING_ADDED) {
    return;
  }

  chromeos::EasyUnlockKeyManager* key_manager =
      chromeos::UserSessionManager::GetInstance()->GetEasyUnlockKeyManager();
  DCHECK(key_manager);

  key_manager->GetDeviceDataList(
      chromeos::UserContext(user_id),
      base::Bind(&EasyUnlockService::OnCryptohomeKeysFetchedForChecking,
                 weak_ptr_factory_.GetWeakPtr(),
                 user_id,
                 paired_devices));
#endif
}

void EasyUnlockService::SetTrialRun() {
  DCHECK(GetType() == TYPE_REGULAR);

  EasyUnlockScreenlockStateHandler* handler = GetScreenlockStateHandler();
  if (handler)
    handler->SetTrialRun();
}

void EasyUnlockService::AddObserver(EasyUnlockServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void EasyUnlockService::RemoveObserver(EasyUnlockServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void  EasyUnlockService::Shutdown() {
  if (shut_down_)
    return;
  shut_down_ = true;

  ShutdownInternal();

  weak_ptr_factory_.InvalidateWeakPtrs();

  ResetScreenlockState();
  bluetooth_detector_.reset();
#if defined(OS_CHROMEOS)
  power_monitor_.reset();
#endif
}

void EasyUnlockService::LoadApp() {
  DCHECK(IsAllowed());

#if defined(OS_CHROMEOS)
  // TODO(xiyuan): Remove this when the app is bundled with chrome.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          proximity_auth::switches::kForceLoadEasyUnlockAppInTests)) {
    return;
  }
#endif

#if defined(GOOGLE_CHROME_BUILD)
  base::FilePath easy_unlock_path;
#if defined(OS_CHROMEOS)
  easy_unlock_path = base::FilePath("/usr/share/chromeos-assets/easy_unlock");
#endif  // defined(OS_CHROMEOS)

#ifndef NDEBUG
  // Only allow app path override switch for debug build.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEasyUnlockAppPath)) {
    easy_unlock_path =
        command_line->GetSwitchValuePath(switches::kEasyUnlockAppPath);
  }
#endif  // !defined(NDEBUG)

  if (!easy_unlock_path.empty()) {
    extensions::ComponentLoader* loader = GetComponentLoader(profile_);
    if (!loader->Exists(extension_misc::kEasyUnlockAppId))
      loader->Add(IDR_EASY_UNLOCK_MANIFEST, easy_unlock_path);

    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    extension_service->EnableExtension(extension_misc::kEasyUnlockAppId);

    NotifyUserUpdated();
  }
#endif  // defined(GOOGLE_CHROME_BUILD)
}

void EasyUnlockService::DisableAppIfLoaded() {
  extensions::ComponentLoader* loader = GetComponentLoader(profile_);
  if (!loader->Exists(extension_misc::kEasyUnlockAppId))
    return;

  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extension_service->DisableExtension(extension_misc::kEasyUnlockAppId,
                                      extensions::Extension::DISABLE_RELOAD);
}

void EasyUnlockService::UnloadApp() {
  GetComponentLoader(profile_)->Remove(extension_misc::kEasyUnlockAppId);
}

void EasyUnlockService::ReloadApp() {
  // Make sure lock screen state set by the extension gets reset.
  ResetScreenlockState();

  if (!GetComponentLoader(profile_)->Exists(extension_misc::kEasyUnlockAppId))
    return;
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile_);
  extension_system->extension_service()->ReloadExtension(
      extension_misc::kEasyUnlockAppId);
  NotifyUserUpdated();
}

void EasyUnlockService::UpdateAppState() {
  if (IsAllowed()) {
    LoadApp();

#if defined(OS_CHROMEOS)
    if (!power_monitor_)
      power_monitor_.reset(new PowerMonitor(this));
#endif
  } else {
    bool bluetooth_waking_up = false;
#if defined(OS_CHROMEOS)
    // If the service is not allowed due to bluetooth not being detected just
    // after system suspend is done, give bluetooth more time to be detected
    // before disabling the app (and resetting screenlock state).
    bluetooth_waking_up =
        power_monitor_.get() && power_monitor_->waking_up() &&
        !bluetooth_detector_->IsPresent();
#endif

    if (!bluetooth_waking_up) {
      DisableAppIfLoaded();
      ResetScreenlockState();
#if defined(OS_CHROMEOS)
      power_monitor_.reset();
#endif
    }
  }
}

void EasyUnlockService::NotifyUserUpdated() {
  std::string user_id = GetUserEmail();
  if (user_id.empty())
    return;

  // Notify the easy unlock app that the user info changed.
  extensions::api::easy_unlock_private::UserInfo info;
  info.user_id = user_id;
  info.logged_in = GetType() == TYPE_REGULAR;
  info.data_ready = info.logged_in || GetRemoteDevices() != NULL;

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(info.ToValue().release());

  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::easy_unlock_private::OnUserInfoUpdated::kEventName,
      args.Pass()));

  extensions::EventRouter::Get(profile_)->DispatchEventToExtension(
       extension_misc::kEasyUnlockAppId, event.Pass());
}

void EasyUnlockService::NotifyTurnOffOperationStatusChanged() {
  FOR_EACH_OBSERVER(
      EasyUnlockServiceObserver, observers_, OnTurnOffOperationStatusChanged());
}

void EasyUnlockService::ResetScreenlockState() {
  screenlock_state_handler_.reset();
  auth_attempt_.reset();
}

void EasyUnlockService::SetScreenlockHardlockedState(
    EasyUnlockScreenlockStateHandler::HardlockState state) {
  if (screenlock_state_handler_)
    screenlock_state_handler_->SetHardlockState(state);
  if (state != EasyUnlockScreenlockStateHandler::NO_HARDLOCK)
    auth_attempt_.reset();
}

void EasyUnlockService::Initialize() {
  InitializeInternal();

#if defined(OS_CHROMEOS)
  // Only start Bluetooth detection for ChromeOS since the feature is
  // only offered on ChromeOS. Enabling this on non-ChromeOS platforms
  // previously introduced a performance regression: http://crbug.com/404482
  // Make sure not to reintroduce a performance regression if re-enabling on
  // additional platforms.
  // TODO(xiyuan): Revisit when non-chromeos platforms are supported.
  bluetooth_detector_->Initialize();
#endif  // defined(OS_CHROMEOS)
}

void EasyUnlockService::OnBluetoothAdapterPresentChanged() {
  UpdateAppState();
}

void EasyUnlockService::SetHardlockStateForUser(
      const std::string& user_id,
      EasyUnlockScreenlockStateHandler::HardlockState state) {
  DCHECK(!user_id.empty());

  PrefService* local_state = GetLocalState();
  if (!local_state)
    return;

  DictionaryPrefUpdate update(local_state, prefs::kEasyUnlockHardlockState);
  update->SetIntegerWithoutPathExpansion(user_id, static_cast<int>(state));

  if (GetUserEmail() == user_id)
    SetScreenlockHardlockedState(state);
}

#if defined(OS_CHROMEOS)
void EasyUnlockService::OnCryptohomeKeysFetchedForChecking(
    const std::string& user_id,
    const std::set<std::string> paired_devices,
    bool success,
    const chromeos::EasyUnlockDeviceKeyDataList& key_data_list) {
  DCHECK(!user_id.empty() && !paired_devices.empty());

  if (!success) {
    SetHardlockStateForUser(user_id,
                            EasyUnlockScreenlockStateHandler::NO_PAIRING);
    return;
  }

  std::set<std::string> devices_in_cryptohome;
  for (const auto& device_key_data : key_data_list)
    devices_in_cryptohome.insert(device_key_data.psk);

  if (paired_devices != devices_in_cryptohome ||
      GetHardlockState() == EasyUnlockScreenlockStateHandler::NO_PAIRING) {
    SetHardlockStateForUser(
        user_id,
        devices_in_cryptohome.empty()
            ? EasyUnlockScreenlockStateHandler::PAIRING_ADDED
            : EasyUnlockScreenlockStateHandler::PAIRING_CHANGED);
  }
}
#endif

void EasyUnlockService::PrepareForSuspend() {
  DisableAppIfLoaded();
  if (screenlock_state_handler_ && screenlock_state_handler_->IsActive()) {
    UpdateScreenlockState(
        EasyUnlockScreenlockStateHandler::STATE_BLUETOOTH_CONNECTING);
  }
}
