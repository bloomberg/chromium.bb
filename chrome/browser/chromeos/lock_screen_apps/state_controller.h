// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_CONTROLLER_H_

#include <memory>
#include <string>

#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/api/app_runtime.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/events/devices/input_device_event_observer.h"

class PrefRegistrySimple;

namespace base {
class TickClock;
}

namespace content {
class BrowserContext;
}

namespace extensions {
class AppDelegate;
class AppWindow;
class Extension;

namespace lock_screen_data {
class LockScreenItemStorage;
}
}  // namespace extensions

namespace session_manager {
class SessionManager;
}

namespace ui {
class InputDeviceManager;
}

namespace lock_screen_apps {

class AppWindowMetricsTracker;
class FocusCyclerDelegate;
class StateObserver;
class FirstAppRunToastManager;

// Manages state of lock screen action handler apps, and notifies
// interested parties as the state changes.
// Currently assumes single supported action - NEW_NOTE.
class StateController : public ash::mojom::TrayActionClient,
                        public session_manager::SessionManagerObserver,
                        public extensions::AppWindowRegistry::Observer,
                        public ui::InputDeviceEventObserver,
                        public chromeos::PowerManagerClient::Observer {
 public:
  // Returns whether the StateController is enabled - it is currently guarded by
  // a feature flag. If not enabled, |StateController| instance is not allowed
  // to be created. |Get| will still work, but it will return nullptr.
  static bool IsEnabled();

  // Returns the global StateController instance. Note that this can return
  // nullptr when lock screen apps are not enabled (see |IsEnabled|).
  static StateController* Get();

  static void RegisterProfilePrefs(PrefRegistrySimple* pref_registry);

  // Note that only one StateController is allowed per process. Creating a
  // StateController will set global instance ptr that can be accessed using
  // |Get|. This pointer will be reset when the StateController is destroyed.
  StateController();
  ~StateController() override;

  // Sets the tray action that should be used by |StateController|.
  // Has to be called before |Initialize|.
  void SetTrayActionPtrForTesting(ash::mojom::TrayActionPtr tray_action_ptr);
  void FlushTrayActionForTesting();
  // Sets the callback that will be run when the state controller is fully
  // initialized and ready for action.
  void SetReadyCallbackForTesting(const base::Closure& ready_callback);
  // Sets the tick clock to be used in tests.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> clock);
  // Sets test AppManager implementation. Should be called before
  // |SetPrimaryProfile|
  void SetAppManagerForTesting(std::unique_ptr<AppManager> app_manager);

  // Initializes mojo bindings for the StateController - it creates binding to
  // ash's tray action interface and sets this object as the interface's client.
  void Initialize();
  void SetPrimaryProfile(Profile* profile);

  // Shuts down the state controller, reseting all dependencies on profiles.
  // Should be called on global instance before profile destruction starts.
  // TODO(tbarzic): Consider removing this after lock screen implementation
  //     moves to ash - the main reason the method is needed is to enable
  //     SigninScreenHandler to safely remove itself as an observer on its
  //     destruction (which might happen after state controller has to be
  //     shutdown). When this is not the case anymore StateController::Shutdown
  //     usage can be replaced with destructing the StateController instance.
  //     https://crbug.com/741145
  void Shutdown();

  void AddObserver(StateObserver* observer);
  void RemoveObserver(StateObserver* observer);

  // Sets the focus cycler delegate the state controller should use to pass on
  // from and give focus to the active lock screen app window.
  void SetFocusCyclerDelegate(FocusCyclerDelegate* delegate);

  // Gets current state assiciated with the lock screen note action.
  ash::mojom::TrayActionState GetLockScreenNoteState() const;

  // ash::mojom::TrayActionClient:
  void RequestNewLockScreenNote(
      ash::mojom::LockScreenNoteOrigin origin) override;
  void CloseLockScreenNote(
      ash::mojom::CloseLockScreenNoteReason reason) override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // extensions::AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

  // ui::InputDeviceEventObserver:
  void OnStylusStateChanged(ui::StylusState state) override;
  void OnTouchscreenDeviceConfigurationChanged() override;

  // chromeos::PowerManagerClient::Observer
  void BrightnessChanged(int level, bool user_initiated) override;
  void SuspendImminent() override;

  // Creates and registers an app window as action handler for the action on
  // Chrome OS lock screen. The ownership of the returned app window is passed
  // to the caller.
  // If the app is not allowed to create an app window for handling the action
  // on lock screen (e.g. if the action has not been requested), it will return
  // nullptr.
  extensions::AppWindow* CreateAppWindowForLockScreenAction(
      content::BrowserContext* context,
      const extensions::Extension* extension,
      extensions::api::app_runtime::ActionType action,
      std::unique_ptr<extensions::AppDelegate> app_delegate);

  // Should be called when the active app window is tabbed through. If needed,
  // the method will take focus from the app window and pass it on using
  // |focus_cycler_delegate_|.
  // Returns whether the focus has been taken from the app window.
  bool HandleTakeFocus(content::WebContents* web_contents, bool reverse);

  // Called from the lock screen Web UI when the animation shown on a note
  // action launch finishes (the animation is started when the lock screen
  // note state changes to kLaunching).
  void NewNoteLaunchAnimationDone();

  FirstAppRunToastManager* first_app_run_toast_manager() {
    return first_app_run_toast_manager_.get();
  }

 private:
  // The screen state determined by observing brightness changes from power
  // manager client.
  enum class ScreenState {
    // The screen state has not yet been initialized.
    kUnknown,
    // The screen is on - i.e. not completely dimmed.
    kOn,
    // The screen is off - it's brightness level is 0.
    kOff
  };

  // Called when profiles needed to run lock screen apps are ready - i.e. when
  // primary user profile was set using |SetPrimaryProfile| and the profile in
  // which app lock screen windows will be run creation is done.
  // |status| - The lock screen profile creation status.
  void OnProfilesReady(Profile* primary_profile,
                       Profile* lock_screen_profile,
                       Profile::CreateStatus status);

  // Gets the encryption key that should be used to encrypt user data created on
  // the lock screen. If a key hadn't previously been created and saved to
  // user prefs, a new key is created and saved.
  // |crypto_key| - the found/created key.
  // Returns whether |crypto_key| was successfully retrieved.
  bool GetUserCryptoKey(Profile* profile, std::string* crypto_key);

  // Continues lock screen apps initialization with primary user profile and
  // associated encryption key to be used for encrypting user data created in
  // lock screen context.
  void InitializeWithCryptoKey(Profile* profile, const std::string& crypto_key);

  // Continues lock screen apps initialization. Should be called when stylus
  // input has been detected.
  void InitializeWithStylusInputPresent();

  // Issues a lock screen note app launch request to |app_manager_|.
  // Expected to be called only in kLaunching state. In the case the launch is
  // not successful, the note taking action state will be changed accordingly.
  void StartLaunchRequest();

  // Called when app manager reports that note taking availability has changed.
  void OnNoteTakingAvailabilityChanged();

  // If there is an app window registered as a handler for note taking action
  // on lock screen, unregisters the window, and closes is if |close_window| is
  // set. It changes the current state to kAvailable or kNotAvailable, depending
  // on whether lock screen note taking action can still be handled.
  void ResetNoteTakingWindowAndMoveToNextState(
      bool close_window,
      ash::mojom::CloseLockScreenNoteReason reason);

  // Requests lock screen note action state change to |state|.
  // Returns whether the action state has changed.
  bool UpdateLockScreenNoteState(ash::mojom::TrayActionState state);

  // Notifies observers that the lock screen note action state changed.
  void NotifyLockScreenNoteStateChanged();

  // Passed as a focus handler to |focus_cycler_delegate_| when the assiciated
  // app window is visible (active or in background).
  // It focuses the app window.
  void FocusAppWindow(bool reverse);

  // Updates the screen state to match the current screen brightness - no-op
  // unless the current screen state is unknown.
  void SetInitialScreenState(double screen_brightness);

  // Updates ths screen state - if the stylus was recently removed and screen
  // has turned on, this will launch a new note action (stylus being removed
  // should launch the note action, but this is deferred if the screen is off
  // when the removal event occurs).
  void SetScreenState(ScreenState screen_state);

  // Lock screen note action state.
  ash::mojom::TrayActionState lock_screen_note_state_ =
      ash::mojom::TrayActionState::kNotAvailable;

  base::ObserverList<StateObserver> observers_;

  mojo::Binding<ash::mojom::TrayActionClient> binding_;
  ash::mojom::TrayActionPtr tray_action_ptr_;

  Profile* lock_screen_profile_ = nullptr;

  // The current screen state.
  ScreenState screen_state_ = ScreenState::kUnknown;

  // The time-stamp of the last observed stylus eject event. This will get set
  // if stylus was ejected while the screen was off, and the note action launch
  // was thus deferred. If the screen is turned on soon after the stylus is
  // ejected, lock screen note action will be launched.
  base::TimeTicks stylus_eject_timestamp_;

  // Whether sending app launch request to the note taking app (using
  // |app_manager_|) was delayed until the note action launch animation is
  // completed by lock screen UI - this is only used with Web UI lock
  // implementation, and for note action launch requests that don't come from
  // the lock UI (i.e. stylus removal).
  bool app_launch_delayed_for_animation_ = false;

  // Whether lock screen apps initialization was stopped due to stylus input
  // missing (or stylus not being otherwise enabled). If stylus availability
  // changes due to stylus input being detected, initialization will continue.
  bool stylus_input_missing_ = false;

  std::unique_ptr<extensions::lock_screen_data::LockScreenItemStorage>
      lock_screen_data_;

  std::unique_ptr<AppManager> app_manager_;

  FocusCyclerDelegate* focus_cycler_delegate_ = nullptr;

  extensions::AppWindow* note_app_window_ = nullptr;
  // Used to track metrics for app window launches - it is set when the user
  // session is locked (and reset on unlock). Note that a single instance
  // should not be reused for different lock sessions - it tracks number of app
  // launches per lock screen.
  std::unique_ptr<AppWindowMetricsTracker> note_app_window_metrics_;

  // Used to show first lock screen app run (toast) dialog when an app window
  // is first launched for an app.
  // NOTE: The manager can be used for every app launch - before showing the
  // toast dialog, the manager will bail out if it determines that the toast
  // for the associated app has been previosly seen (and closed) by the user.
  std::unique_ptr<FirstAppRunToastManager> first_app_run_toast_manager_;

  ScopedObserver<extensions::AppWindowRegistry,
                 extensions::AppWindowRegistry::Observer>
      app_window_observer_;
  ScopedObserver<session_manager::SessionManager,
                 session_manager::SessionManagerObserver>
      session_observer_;
  ScopedObserver<ui::InputDeviceManager, ui::InputDeviceEventObserver>
      input_devices_observer_;
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_;

  // If set, this callback will be run when the state controller is fully
  // initialized. It can be used to throttle tests until state controller
  // is ready for action - i.e. until the state controller starts reacting
  // to session / app manager changes.
  base::Closure ready_callback_;

  // The clock used to keep track of time, for example to report app window
  // lifetime metrics.
  std::unique_ptr<base::TickClock> tick_clock_;

  base::WeakPtrFactory<StateController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StateController);
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_CONTROLLER_H_
