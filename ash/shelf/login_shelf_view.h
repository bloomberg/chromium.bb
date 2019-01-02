// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_LOGIN_SHELF_VIEW_H_
#define ASH_SHELF_LOGIN_SHELF_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/lock_screen_action/lock_screen_action_background_observer.h"
#include "ash/login/login_screen_controller_observer.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/interfaces/kiosk_app_info.mojom.h"
#include "ash/public/interfaces/login_screen.mojom.h"
#include "ash/shutdown_controller.h"
#include "ash/tray_action/tray_action_observer.h"
#include "base/scoped_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}

namespace session_manager {
enum class SessionState;
}

namespace ash {

class LockScreenActionBackgroundController;
enum class LockScreenActionBackgroundState;
class TrayAction;
class LoginScreenController;

class KioskAppsButton;

// LoginShelfView contains the shelf buttons visible outside of an active user
// session. ShelfView and LoginShelfView should never be shown together.
// This view is attached as a LoginDataDispatcher::Observer when the LockScreen
// is instantiated in kLogin mode. It cannot attach itself because it does not
// know when the Login is instantiated.
class ASH_EXPORT LoginShelfView : public views::View,
                                  public views::ButtonListener,
                                  public TrayActionObserver,
                                  public LockScreenActionBackgroundObserver,
                                  public ShutdownController::Observer,
                                  public LoginScreenControllerObserver,
                                  public LoginDataDispatcher::Observer {
 public:
  enum ButtonId {
    kShutdown = 1,   // Shut down the device.
    kRestart,        // Restart the device.
    kSignOut,        // Sign out the active user session.
    kCloseNote,      // Close the lock screen note.
    kCancel,         // Cancel multiple user sign-in.
    kBrowseAsGuest,  // Use in guest mode.
    kAddUser,        // Add a new user.
    kApps,           // Show list of available kiosk apps.
    kParentAccess    // Unlock child device with Parent Access Code.
  };

  explicit LoginShelfView(
      LockScreenActionBackgroundController* lock_screen_action_background);
  ~LoginShelfView() override;

  // ShelfWidget observes SessionController for higher-level UI changes and
  // then notifies LoginShelfView to update its own UI.
  void UpdateAfterSessionStateChange(session_manager::SessionState state);

  // Sets the list of kiosk apps that can be launched from the login shelf.
  void SetKioskApps(std::vector<mojom::KioskAppInfoPtr> kiosk_apps);

  // Sets the state of the login dialog.
  void SetLoginDialogState(mojom::OobeDialogState state);

  // Sets if the guest button on the login shelf can be shown. Even if set to
  // true the button may still not be visible.
  void SetAllowLoginAsGuest(bool allow_guest);

  // Sets whether parent access button can be shown on the login shelf.
  void SetShowParentAccess(bool show);

  // Sets if the guest button on the login shelf can be shown during gaia
  // signin screen.
  void SetShowGuestButtonInOobe(bool show);

  // Sets whether users can be added from the login screen.
  void SetAddUserButtonEnabled(bool enable_add_user);

  // Sets whether shutdown button is enabled in the login screen.
  void SetShutdownButtonEnabled(bool enable_shutdown_button);

  // views::View:
  const char* GetClassName() const override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 protected:
  // TrayActionObserver:
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;

  // LockScreenActionBackgroundObserver:
  void OnLockScreenActionBackgroundStateChanged(
      LockScreenActionBackgroundState state) override;

  // ShutdownController::Observer:
  void OnShutdownPolicyChanged(bool reboot_on_shutdown) override;

  // LoginScreenControllerObserver:
  void OnOobeDialogStateChanged(mojom::OobeDialogState state) override;

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(
      const std::vector<mojom::LoginUserInfoPtr>& users) override;

 private:
  bool LockScreenActionBackgroundAnimating() const;

  // Updates the visibility of buttons based on state changes, e.g. shutdown
  // policy updates, session state changes etc.
  void UpdateUi();

  mojom::OobeDialogState dialog_state_ = mojom::OobeDialogState::HIDDEN;
  bool allow_guest_ = true;
  bool allow_guest_in_oobe_ = false;
  bool show_parent_access_ = false;
  // When the Gaia screen is active during Login, the guest-login button should
  // appear if there are no user views.
  bool login_screen_has_users_ = false;

  LockScreenActionBackgroundController* lock_screen_action_background_;

  ScopedObserver<TrayAction, TrayActionObserver> tray_action_observer_;

  ScopedObserver<LockScreenActionBackgroundController,
                 LockScreenActionBackgroundObserver>
      lock_screen_action_background_observer_;

  ScopedObserver<ShutdownController, ShutdownController::Observer>
      shutdown_controller_observer_;

  ScopedObserver<LoginScreenController, LoginScreenControllerObserver>
      login_screen_controller_observer_;

  KioskAppsButton* kiosk_apps_button_ = nullptr;  // Owned by view hierarchy

  DISALLOW_COPY_AND_ASSIGN(LoginShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_LOGIN_SHELF_VIEW_H_
