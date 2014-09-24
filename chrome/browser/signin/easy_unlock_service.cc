// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/easy_unlock_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
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
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"
#include "grit/browser_resources.h"

#if defined(OS_CHROMEOS)
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

}  // namespace

// static
EasyUnlockService* EasyUnlockService::Get(Profile* profile) {
  return EasyUnlockServiceFactory::GetForProfile(profile);
}

class EasyUnlockService::BluetoothDetector
    : public device::BluetoothAdapter::Observer {
 public:
  explicit BluetoothDetector(EasyUnlockService* service)
      : service_(service),
        weak_ptr_factory_(this) {
  }

  virtual ~BluetoothDetector() {
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
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE {
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
class EasyUnlockService::PowerMonitor :
  public chromeos::PowerManagerClient::Observer {
 public:
  explicit PowerMonitor(EasyUnlockService* service) : service_(service) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        AddObserver(this);
  }

  virtual ~PowerMonitor() {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        RemoveObserver(this);
  }

 private:
  // chromeos::PowerManagerClient::Observer:
  virtual void SuspendImminent() OVERRIDE {
    service_->DisableAppIfLoaded();
  }

  virtual void SuspendDone(const base::TimeDelta& sleep_duration) OVERRIDE {
    service_->LoadApp();
  }

  EasyUnlockService* service_;

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
      prefs::kEasyUnlockEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEasyUnlockShowTutorial,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kEasyUnlockPairing,
      new base::DictionaryValue(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kEasyUnlockAllowed,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool EasyUnlockService::IsAllowed() {
  if (shut_down_)
    return false;

  if (!IsAllowedInternal())
    return false;

#if defined(OS_CHROMEOS)
  if (!profile_->GetPrefs()->GetBoolean(prefs::kEasyUnlockAllowed))
    return false;

  // Respect existing policy and skip finch test.
  if (!profile_->GetPrefs()->IsManagedPreference(prefs::kEasyUnlockAllowed)) {
    // It is disabled when the trial exists and is in "Disable" group.
    if (base::FieldTrialList::FindFullName("EasyUnlock") == "Disable")
      return false;
  }

  if (!bluetooth_detector_->IsPresent())
    return false;

  return true;
#else
  // TODO(xiyuan): Revisit when non-chromeos platforms are supported.
  return false;
#endif
}

EasyUnlockScreenlockStateHandler*
    EasyUnlockService::GetScreenlockStateHandler() {
  if (!IsAllowed())
    return NULL;
  if (!screenlock_state_handler_) {
    screenlock_state_handler_.reset(new EasyUnlockScreenlockStateHandler(
        GetUserEmail(),
        GetType() == TYPE_REGULAR ? profile_->GetPrefs() : NULL,
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

  if (state != EasyUnlockScreenlockStateHandler::STATE_AUTHENTICATED)
    auth_attempt_.reset();
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
  if (auth_attempt_)
    auth_attempt_->FinalizeUnlock(GetUserEmail(), success);
  auth_attempt_.reset();
}

void EasyUnlockService::FinalizeSignin(const std::string& key) {
  if (!auth_attempt_)
    return;
  std::string wrapped_secret = GetWrappedSecret();
  if (!wrapped_secret.empty())
    auth_attempt_->FinalizeSignin(GetUserEmail(), wrapped_secret, key);
  auth_attempt_.reset();
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
  // Make sure lock screen state set by the extension gets reset.
  ResetScreenlockState();

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
    DisableAppIfLoaded();
#if defined(OS_CHROMEOS)
    power_monitor_.reset();
#endif
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

