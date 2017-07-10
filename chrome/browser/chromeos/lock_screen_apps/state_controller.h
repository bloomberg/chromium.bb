// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_CONTROLLER_H_

#include <memory>

#include "ash/public/interfaces/tray_action.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager.h"
#include "chrome/browser/chromeos/lock_screen_apps/state_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/api/app_runtime.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class AppDelegate;
class AppWindow;
class Extension;
}  // namespace extensions

namespace session_manager {
class SessionManager;
}

namespace ui {
class InputDeviceManager;
}

namespace lock_screen_apps {

class StateObserver;

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
  // Sets test AppManager implementation. Should be called before
  // |SetPrimaryProfile|
  void SetAppManagerForTesting(std::unique_ptr<AppManager> app_manager);

  // Initializes mojo bindings for the StateController - it creates binding to
  // ash's tray action interface and sets this object as the interface's client.
  void Initialize();
  void SetPrimaryProfile(Profile* profile);

  void AddObserver(StateObserver* observer);
  void RemoveObserver(StateObserver* observer);

  // Gets current state assiciated with the lock screen note action.
  ash::mojom::TrayActionState GetLockScreenNoteState() const;

  // ash::mojom::TrayActionClient:
  void RequestNewLockScreenNote() override;

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // extensions::AppWindowRegistry::Observer:
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

  // ui::InputDeviceEventObserver:
  void OnStylusStateChanged(ui::StylusState state) override;

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

  // If there are any active lock screen action handlers, moved their windows
  // to background, to ensure lock screen UI is visible.
  void MoveToBackground();

  // If there are any lock screen action handler in background, moves their
  // windows back to foreground (i.e. visible over lock screen UI).
  void MoveToForeground();

 private:
  // Called when profiles needed to run lock screen apps are ready - i.e. when
  // primary user profile was set using |SetPrimaryProfile| and the profile in
  // which app lock screen windows will be run creation is done.
  // |status| - The lock screen profile creation status.
  void OnProfilesReady(Profile* primary_profile,
                       Profile* lock_screen_profile,
                       Profile::CreateStatus status);

  // Called when app manager reports that note taking availability has changed.
  void OnNoteTakingAvailabilityChanged();

  // If there is an app window registered as a handler for note taking action
  // on lock screen, unregisters the window, and closes is if |close_window| is
  // set. It changes the current state to kAvailable or kNotAvailable, depending
  // on whether lock screen note taking action can still be handled.
  void ResetNoteTakingWindowAndMoveToNextState(bool close_window);

  // Requests lock screen note action state change to |state|.
  // Returns whether the action state has changed.
  bool UpdateLockScreenNoteState(ash::mojom::TrayActionState state);

  // Notifies observers that the lock screen note action state changed.
  void NotifyLockScreenNoteStateChanged();

  // Lock screen note action state.
  ash::mojom::TrayActionState lock_screen_note_state_ =
      ash::mojom::TrayActionState::kNotAvailable;

  base::ObserverList<StateObserver> observers_;

  mojo::Binding<ash::mojom::TrayActionClient> binding_;
  ash::mojom::TrayActionPtr tray_action_ptr_;

  Profile* lock_screen_profile_ = nullptr;

  std::unique_ptr<AppManager> app_manager_;

  extensions::AppWindow* note_app_window_ = nullptr;

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

  base::WeakPtrFactory<StateController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StateController);
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_STATE_CONTROLLER_H_
