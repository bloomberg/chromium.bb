// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/lock_screen_apps/state_controller.h"

#include <utility>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/stylus_utils.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/wm/window_animations.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager_impl.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_window_metrics_tracker.h"
#include "chrome/browser/chromeos/lock_screen_apps/first_app_run_toast_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/focus_cycler_delegate.h"
#include "chrome/browser/chromeos/note_taking_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "crypto/symmetric_key.h"
#include "extensions/browser/api/lock_screen_data/lock_screen_item_storage.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/extension.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/devices/stylus_state.h"

using ash::mojom::CloseLockScreenNoteReason;
using ash::mojom::LockScreenNoteOrigin;
using ash::mojom::TrayActionState;

namespace lock_screen_apps {

namespace {

// The time span a stylus eject is considered valid - i.e. the time period
// within a stylus eject event can cause a lock screen note launch.
constexpr int kStylusEjectValidityMs = 1000;

// Key for user pref that contains the 256 bit AES key that should be used to
// encrypt persisted user data created on the lock screen.
constexpr char kDataCryptoKeyPref[] = "lockScreenAppDataCryptoKey";

StateController* g_instance = nullptr;

// Generates a random 256 bit AES key. Returns an empty string on error.
std::string GenerateCryptoKey() {
  std::unique_ptr<crypto::SymmetricKey> symmetric_key =
      crypto::SymmetricKey::GenerateRandomKey(crypto::SymmetricKey::AES, 256);
  if (!symmetric_key)
    return "";
  return symmetric_key->key();
}

}  // namespace

// static
bool StateController::IsEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kDisableLockScreenApps);
}

// static
StateController* StateController::Get() {
  DCHECK(g_instance || !IsEnabled());
  return g_instance;
}

// static
void StateController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kDataCryptoKeyPref, "");
}

StateController::StateController()
    : binding_(this),
      app_window_observer_(this),
      session_observer_(this),
      input_devices_observer_(this),
      power_manager_client_observer_(this),
      weak_ptr_factory_(this) {
  DCHECK(!g_instance);
  DCHECK(IsEnabled());

  g_instance = this;
}

StateController::~StateController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void StateController::SetTrayActionPtrForTesting(
    ash::mojom::TrayActionPtr tray_action_ptr) {
  tray_action_ptr_ = std::move(tray_action_ptr);
}

void StateController::FlushTrayActionForTesting() {
  tray_action_ptr_.FlushForTesting();
}

void StateController::SetReadyCallbackForTesting(
    const base::Closure& ready_callback) {
  DCHECK(ready_callback_.is_null());

  ready_callback_ = ready_callback;
}

void StateController::SetTickClockForTesting(
    std::unique_ptr<base::TickClock> clock) {
  DCHECK(!tick_clock_);
  tick_clock_ = std::move(clock);
}

void StateController::SetAppManagerForTesting(
    std::unique_ptr<AppManager> app_manager) {
  DCHECK(!app_manager_);
  app_manager_ = std::move(app_manager);
}

void StateController::Initialize() {
  if (!tick_clock_)
    tick_clock_ = base::MakeUnique<base::DefaultTickClock>();

  // The tray action ptr might be set previously if the client was being created
  // for testing.
  if (!tray_action_ptr_) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    connector->BindInterface(ash::mojom::kServiceName, &tray_action_ptr_);
  }
  ash::mojom::TrayActionClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  tray_action_ptr_->SetClient(std::move(client), lock_screen_note_state_);
}

void StateController::SetPrimaryProfile(Profile* profile) {
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || !user->HasGaiaAccount()) {
    if (!ready_callback_.is_null())
      std::move(ready_callback_).Run();
    return;
  }

  g_browser_process->profile_manager()->CreateProfileAsync(
      chromeos::ProfileHelper::GetLockScreenAppProfilePath(),
      base::Bind(&StateController::OnProfilesReady,
                 weak_ptr_factory_.GetWeakPtr(), profile),
      base::string16() /* name */, "" /* icon_url*/,
      "" /* supervised_user_id */);
}

void StateController::Shutdown() {
  session_observer_.RemoveAll();
  lock_screen_data_.reset();
  if (app_manager_) {
    app_manager_->Stop();
    ResetNoteTakingWindowAndMoveToNextState(
        true /*close_window*/, CloseLockScreenNoteReason::kShutdown);
    app_manager_.reset();
  }
  first_app_run_toast_manager_.reset();
  focus_cycler_delegate_ = nullptr;
  power_manager_client_observer_.RemoveAll();
  input_devices_observer_.RemoveAll();
  binding_.Close();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void StateController::OnProfilesReady(Profile* primary_profile,
                                      Profile* lock_screen_profile,
                                      Profile::CreateStatus status) {
  // Ignore CREATED status - wait for profile to be initialized before
  // continuing.
  if (status == Profile::CREATE_STATUS_CREATED) {
    // Disable safe browsing for the profile to avoid activating
    // SafeBrowsingService when the user has safe browsing disabled (reasoning
    // similar to http://crbug.com/461493).
    // TODO(tbarzic): Revisit this if webviews get enabled for lock screen apps.
    lock_screen_profile->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                false);
    return;
  }

  // On error, bail out - this will cause the lock screen apps to remain
  // unavailable on the device.
  if (status != Profile::CREATE_STATUS_INITIALIZED) {
    LOG(ERROR) << "Failed to create profile for lock screen apps.";
    return;
  }

  DCHECK(!lock_screen_profile_);

  lock_screen_profile_ = lock_screen_profile;
  lock_screen_profile_->GetPrefs()->SetBoolean(prefs::kForceEphemeralProfiles,
                                               true);

  std::string key;
  if (!GetUserCryptoKey(primary_profile, &key)) {
    LOG(ERROR) << "Failed to get crypto key for user lock screen apps.";
    return;
  }

  InitializeWithCryptoKey(primary_profile, key);
}

bool StateController::GetUserCryptoKey(Profile* profile, std::string* key) {
  *key = profile->GetPrefs()->GetString(kDataCryptoKeyPref);
  if (!key->empty() && base::Base64Decode(*key, key))
    return true;

  *key = GenerateCryptoKey();

  if (key->empty())
    return false;

  std::string base64_encoded_key;
  base::Base64Encode(*key, &base64_encoded_key);

  profile->GetPrefs()->SetString(kDataCryptoKeyPref, base64_encoded_key);
  return true;
}

void StateController::InitializeWithCryptoKey(Profile* profile,
                                              const std::string& crypto_key) {
  base::FilePath base_path;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &base_path)) {
    LOG(ERROR) << "Failed to get base storage dir for lock screen app data.";
    return;
  }

  lock_screen_data_ =
      base::MakeUnique<extensions::lock_screen_data::LockScreenItemStorage>(
          profile, g_browser_process->local_state(), crypto_key,
          base_path.AppendASCII("lock_screen_app_data"));
  lock_screen_data_->SetSessionLocked(false);

  chromeos::NoteTakingHelper::Get()->SetProfileWithEnabledLockScreenApps(
      profile);

  // App manager might have been set previously by a test.
  if (!app_manager_)
    app_manager_ = base::MakeUnique<AppManagerImpl>(tick_clock_.get());
  app_manager_->Initialize(profile, lock_screen_profile_->GetOriginalProfile());

  first_app_run_toast_manager_ =
      std::make_unique<FirstAppRunToastManager>(profile);

  input_devices_observer_.Add(ui::InputDeviceManager::GetInstance());

  // Do not start state controller if stylus input is not present as lock
  // screen notes apps are geared towards stylus.
  // State controller will observe inpt device changes and continue
  // initialization if stylus input is found.
  if (!ash::stylus_utils::HasStylusInput()) {
    stylus_input_missing_ = true;

    if (!ready_callback_.is_null())
      std::move(ready_callback_).Run();
    return;
  }

  InitializeWithStylusInputPresent();
}

void StateController::InitializeWithStylusInputPresent() {
  stylus_input_missing_ = false;

  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->GetScreenBrightnessPercent(
          base::Bind(&StateController::SetInitialScreenState,
                     weak_ptr_factory_.GetWeakPtr()));
  power_manager_client_observer_.Add(
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
  session_observer_.Add(session_manager::SessionManager::Get());
  OnSessionStateChanged();

  // SessionController is fully initialized at this point.
  if (!ready_callback_.is_null())
    std::move(ready_callback_).Run();
}

void StateController::AddObserver(StateObserver* observer) {
  observers_.AddObserver(observer);
}

void StateController::RemoveObserver(StateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void StateController::SetFocusCyclerDelegate(FocusCyclerDelegate* delegate) {
  DCHECK(!focus_cycler_delegate_ || !delegate);

  if (focus_cycler_delegate_ && note_app_window_)
    focus_cycler_delegate_->UnregisterLockScreenAppFocusHandler();

  focus_cycler_delegate_ = delegate;

  if (focus_cycler_delegate_ && note_app_window_) {
    focus_cycler_delegate_->RegisterLockScreenAppFocusHandler(base::Bind(
        &StateController::FocusAppWindow, weak_ptr_factory_.GetWeakPtr()));
  }
}

TrayActionState StateController::GetLockScreenNoteState() const {
  return lock_screen_note_state_;
}

void StateController::RequestNewLockScreenNote(LockScreenNoteOrigin origin) {
  if (lock_screen_note_state_ != TrayActionState::kAvailable)
    return;

  DCHECK(app_manager_->IsNoteTakingAppAvailable());

  UMA_HISTOGRAM_ENUMERATION("Apps.LockScreen.NoteTakingApp.LaunchRequestReason",
                            origin, LockScreenNoteOrigin::kCount);

  // Update state to launching even if app fails to launch - this is to notify
  // listeners that a lock screen note request was handled.
  UpdateLockScreenNoteState(TrayActionState::kLaunching);

  // Defer app launch for stylus eject with Web UI based lock screen until the
  // note action launch animation finishes. The goal is to ensure the app
  // window is not shown before the animation completes.
  // This is not needed for views based lock screen - the views implementation
  // already ensures UI animation is done before the app window is shown.
  // This is not needed for requests that come from the lock UI as the lock
  // screen UI sends these requests *after* the note action launch animation.
  if (origin == LockScreenNoteOrigin::kStylusEject &&
      !ash::switches::IsUsingMdLogin()) {
    app_launch_delayed_for_animation_ = true;
    return;
  }

  StartLaunchRequest();
}

void StateController::StartLaunchRequest() {
  DCHECK_EQ(lock_screen_note_state_, TrayActionState::kLaunching);

  if (!app_manager_->LaunchNoteTaking()) {
    UpdateLockScreenNoteState(TrayActionState::kAvailable);
    return;
  }

  note_app_window_metrics_->AppLaunchRequested();
}

void StateController::CloseLockScreenNote(CloseLockScreenNoteReason reason) {
  ResetNoteTakingWindowAndMoveToNextState(true /*close_window*/, reason);
}

void StateController::OnSessionStateChanged() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
    lock_screen_data_->SetSessionLocked(false);
    app_manager_->Stop();
    ResetNoteTakingWindowAndMoveToNextState(
        true /*close_window*/, CloseLockScreenNoteReason::kSessionUnlock);
    note_app_window_metrics_.reset();
    return;
  }

  // base::Unretained is safe here because |app_manager_| is owned by |this|,
  // and the callback will not be invoked after |app_manager_| goes out of
  // scope.
  app_manager_->Start(
      base::Bind(&StateController::OnNoteTakingAvailabilityChanged,
                 base::Unretained(this)));
  note_app_window_metrics_ =
      base::MakeUnique<AppWindowMetricsTracker>(tick_clock_.get());
  lock_screen_data_->SetSessionLocked(true);
  OnNoteTakingAvailabilityChanged();
}

void StateController::OnAppWindowAdded(extensions::AppWindow* app_window) {
  if (note_app_window_ != app_window)
    return;
  first_app_run_toast_manager_->RunForAppWindow(note_app_window_);
  note_app_window_metrics_->AppWindowCreated(app_window);
}

void StateController::OnAppWindowRemoved(extensions::AppWindow* app_window) {
  if (note_app_window_ != app_window)
    return;
  ResetNoteTakingWindowAndMoveToNextState(
      false /*close_window*/, CloseLockScreenNoteReason::kAppWindowClosed);
}

void StateController::OnStylusStateChanged(ui::StylusState state) {
  if (lock_screen_note_state_ != TrayActionState::kAvailable)
    return;

  if (state != ui::StylusState::REMOVED) {
    stylus_eject_timestamp_ = base::TimeTicks();
    return;
  }

  if (screen_state_ == ScreenState::kOn) {
    RequestNewLockScreenNote(LockScreenNoteOrigin::kStylusEject);
  } else {
    stylus_eject_timestamp_ = tick_clock_->NowTicks();
  }
}

void StateController::OnTouchscreenDeviceConfigurationChanged() {
  if (stylus_input_missing_ && ash::stylus_utils::HasStylusInput())
    InitializeWithStylusInputPresent();
}

void StateController::BrightnessChanged(int level, bool user_initiated) {
  if (level == 0 && !user_initiated) {
    ResetNoteTakingWindowAndMoveToNextState(
        true /*close_window*/, CloseLockScreenNoteReason::kScreenDimmed);
  }

  SetScreenState(level == 0 ? ScreenState::kOff : ScreenState::kOn);
}

void StateController::SuspendImminent() {
  ResetNoteTakingWindowAndMoveToNextState(true /*close_window*/,
                                          CloseLockScreenNoteReason::kSuspend);
}

extensions::AppWindow* StateController::CreateAppWindowForLockScreenAction(
    content::BrowserContext* context,
    const extensions::Extension* extension,
    extensions::api::app_runtime::ActionType action,
    std::unique_ptr<extensions::AppDelegate> app_delegate) {
  if (action != extensions::api::app_runtime::ACTION_TYPE_NEW_NOTE)
    return nullptr;

  if (lock_screen_note_state_ != TrayActionState::kLaunching)
    return nullptr;

  if (!lock_screen_profile_->IsSameProfile(
          Profile::FromBrowserContext(context))) {
    return nullptr;
  }

  if (!extension || app_manager_->GetNoteTakingAppId() != extension->id())
    return nullptr;

  // The ownership of the window is passed to the caller of this method.
  note_app_window_ =
      new extensions::AppWindow(context, app_delegate.release(), extension);
  app_window_observer_.Add(
      extensions::AppWindowRegistry::Get(lock_screen_profile_));
  UpdateLockScreenNoteState(TrayActionState::kActive);
  if (focus_cycler_delegate_) {
    focus_cycler_delegate_->RegisterLockScreenAppFocusHandler(base::Bind(
        &StateController::FocusAppWindow, weak_ptr_factory_.GetWeakPtr()));
  }
  return note_app_window_;
}

bool StateController::HandleTakeFocus(content::WebContents* web_contents,
                                      bool reverse) {
  if (!focus_cycler_delegate_ ||
      GetLockScreenNoteState() != TrayActionState::kActive ||
      note_app_window_->web_contents() != web_contents) {
    return false;
  }

  focus_cycler_delegate_->HandleLockScreenAppFocusOut(reverse);
  return true;
}

void StateController::NewNoteLaunchAnimationDone() {
  if (!app_launch_delayed_for_animation_)
    return;
  DCHECK_EQ(TrayActionState::kLaunching, lock_screen_note_state_);

  app_launch_delayed_for_animation_ = false;
  StartLaunchRequest();
}

void StateController::OnNoteTakingAvailabilityChanged() {
  if (!app_manager_->IsNoteTakingAppAvailable() ||
      (note_app_window_ && note_app_window_->GetExtension()->id() !=
                               app_manager_->GetNoteTakingAppId())) {
    ResetNoteTakingWindowAndMoveToNextState(
        true /*close_window*/,
        CloseLockScreenNoteReason::kAppLockScreenSupportDisabled);
    return;
  }

  if (GetLockScreenNoteState() == TrayActionState::kNotAvailable)
    UpdateLockScreenNoteState(TrayActionState::kAvailable);
}

void StateController::FocusAppWindow(bool reverse) {
  // If the app window is not active, pass the focus on to the delegate..
  if (GetLockScreenNoteState() != TrayActionState::kActive) {
    focus_cycler_delegate_->HandleLockScreenAppFocusOut(reverse);
    return;
  }

  note_app_window_->web_contents()->FocusThroughTabTraversal(reverse);
  note_app_window_->GetBaseWindow()->Activate();
  note_app_window_->web_contents()->Focus();
}

void StateController::SetInitialScreenState(double screen_brightness) {
  if (screen_state_ != ScreenState::kUnknown)
    return;

  SetScreenState(screen_brightness == 0 ? ScreenState::kOff : ScreenState::kOn);
}

void StateController::SetScreenState(ScreenState screen_state) {
  if (screen_state_ == screen_state)
    return;

  screen_state_ = screen_state;

  if (screen_state_ == ScreenState::kOn && !stylus_eject_timestamp_.is_null() &&
      tick_clock_->NowTicks() - stylus_eject_timestamp_ <
          base::TimeDelta::FromMilliseconds(kStylusEjectValidityMs)) {
    stylus_eject_timestamp_ = base::TimeTicks();
    RequestNewLockScreenNote(LockScreenNoteOrigin::kStylusEject);
  }
}

void StateController::ResetNoteTakingWindowAndMoveToNextState(
    bool close_window,
    CloseLockScreenNoteReason reason) {
  app_window_observer_.RemoveAll();
  stylus_eject_timestamp_ = base::TimeTicks();
  app_launch_delayed_for_animation_ = false;
  if (first_app_run_toast_manager_)
    first_app_run_toast_manager_->Reset();

  if (note_app_window_metrics_)
    note_app_window_metrics_->Reset();

  if (lock_screen_note_state_ != TrayActionState::kAvailable &&
      lock_screen_note_state_ != TrayActionState::kNotAvailable) {
    UMA_HISTOGRAM_ENUMERATION(
        "Apps.LockScreen.NoteTakingApp.NoteTakingExitReason", reason,
        CloseLockScreenNoteReason::kCount);
  }

  if (note_app_window_) {
    if (focus_cycler_delegate_)
      focus_cycler_delegate_->UnregisterLockScreenAppFocusHandler();

    if (close_window && note_app_window_->GetBaseWindow()) {
      // Whenever we close the window we want to immediately hide it without
      // animating, as the underlying UI implements a special animation. If we
      // also animate the window the animations will conflict.
      ::wm::SetWindowVisibilityAnimationTransition(
          note_app_window_->GetNativeWindow(), ::wm::ANIMATE_NONE);
      note_app_window_->GetBaseWindow()->Close();
    }
    note_app_window_ = nullptr;
  }

  UpdateLockScreenNoteState(app_manager_->IsNoteTakingAppAvailable()
                                ? TrayActionState::kAvailable
                                : TrayActionState::kNotAvailable);
}

bool StateController::UpdateLockScreenNoteState(TrayActionState state) {
  const TrayActionState old_state = GetLockScreenNoteState();
  if (old_state == state)
    return false;

  lock_screen_note_state_ = state;
  NotifyLockScreenNoteStateChanged();
  return true;
}

void StateController::NotifyLockScreenNoteStateChanged() {
  for (auto& observer : observers_)
    observer.OnLockScreenNoteStateChanged(lock_screen_note_state_);

  tray_action_ptr_->UpdateLockScreenNoteState(lock_screen_note_state_);
}

}  // namespace lock_screen_apps
