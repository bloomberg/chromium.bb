// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_H_
#define ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/login/lock_screen_apps_focus_observer.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/login/ui/login_display_style.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/system_tray_focus_observer.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/view.h"

namespace keyboard {
class KeyboardController;
}  // namespace keyboard

namespace views {
class BoxLayout;
class StyledLabel;
}  // namespace views

namespace ash {

class LoginAuthUserView;
class LoginBubble;
class LoginDetachableBaseModel;
class NoteActionLaunchButton;
class ScrollableUsersListView;

namespace mojom {
enum class TrayActionState;
}

// LockContentsView hosts the root view for the lock screen. All other lock
// screen views are embedded within this one. LockContentsView is per-display,
// but it is always shown on the primary display. There is only one instance
// at a time.
class ASH_EXPORT LockContentsView : public NonAccessibleView,
                                    public LockScreenAppsFocusObserver,
                                    public LoginDataDispatcher::Observer,
                                    public SystemTrayFocusObserver,
                                    public display::DisplayObserver,
                                    public views::StyledLabelListener,
                                    public SessionObserver,
                                    public keyboard::KeyboardControllerObserver,
                                    public ShellObserver {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LockContentsView* view);
    ~TestApi();

    LoginAuthUserView* primary_auth() const;
    LoginAuthUserView* opt_secondary_auth() const;
    ScrollableUsersListView* users_list() const;
    views::View* note_action() const;
    LoginBubble* tooltip_bubble() const;
    LoginBubble* auth_error_bubble() const;
    LoginBubble* detachable_base_error_bubble() const;
    views::View* dev_channel_info() const;

   private:
    LockContentsView* const view_;
  };

  LockContentsView(
      mojom::TrayActionState initial_note_action_state,
      LoginDataDispatcher* data_dispatcher,
      std::unique_ptr<LoginDetachableBaseModel> detachable_base_model);
  ~LockContentsView() override;

  // views::View:
  void Layout() override;
  void AddedToWidget() override;
  void OnFocus() override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // LockScreenAppsFocusObserver:
  void OnFocusLeavingLockScreenApps(bool reverse) override;

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(
      const std::vector<mojom::LoginUserInfoPtr>& users) override;
  void OnPinEnabledForUserChanged(const AccountId& user, bool enabled) override;
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override;
  void OnClickToUnlockEnabledForUserChanged(const AccountId& user,
                                            bool enabled) override;
  void OnShowEasyUnlockIcon(
      const AccountId& user,
      const mojom::EasyUnlockIconOptionsPtr& icon) override;
  void OnDevChannelInfoChanged(const std::string& os_version_label_text,
                               const std::string& enterprise_info_text,
                               const std::string& bluetooth_name) override;
  void OnPublicSessionDisplayNameChanged(
      const AccountId& account_id,
      const std::string& display_name) override;
  void OnPublicSessionLocalesChanged(const AccountId& account_id,
                                     const base::ListValue& locales,
                                     const std::string& default_locale,
                                     bool show_advanced_view) override;
  void OnDetachableBasePairingStatusChanged(
      DetachableBasePairingStatus pairing_status) override;

  // SystemTrayFocusObserver:
  void OnFocusLeavingSystemTray(bool reverse) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override{};
  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // ash::ShellObserver:
  void OnVirtualKeyboardStateChanged(bool activated,
                                     aura::Window* root_window) override;

  // keyboard::KeyboardControllerObserver:
  void OnStateChanged(const keyboard::KeyboardControllerState state) override;

 private:
  class UserState {
   public:
    explicit UserState(AccountId account_id);
    UserState(UserState&&);
    ~UserState();

    AccountId account_id;
    bool show_pin = false;
    bool enable_tap_auth = false;
    mojom::EasyUnlockIconOptionsPtr easy_unlock_state;

   private:
    DISALLOW_COPY_AND_ASSIGN(UserState);
  };

  using OnRotate = base::RepeatingCallback<void(bool landscape)>;

  // Focus the next/previous widget.
  void FocusNextWidget(bool reverse);

  // 1-2 users.
  void CreateLowDensityLayout(
      const std::vector<mojom::LoginUserInfoPtr>& users);
  // 3-6 users.
  void CreateMediumDensityLayout(
      const std::vector<mojom::LoginUserInfoPtr>& users);
  // 7+ users.
  void CreateHighDensityLayout(
      const std::vector<mojom::LoginUserInfoPtr>& users);

  // Lay out the entire view. This is called when the view is attached to a
  // widget and when the screen is rotated.
  void DoLayout();

  // Lay out the top header. This is called when the children of the top header
  // change contents or visibility.
  void LayoutTopHeader();

  // Creates a new view with |landscape| and |portrait| preferred sizes.
  // |landscape| and |portrait| specify the width of the preferred size; the
  // height is an arbitrary non-zero value. The correct size is chosen
  // dynamically based on screen orientation. The view will respond to
  // orientation changes.
  views::View* MakeOrientationViewWithWidths(int landscape, int portrait);

  // Adds |on_rotate| to |rotation_actions_| and immediately executes it with
  // the current rotation.
  void AddRotationAction(const OnRotate& on_rotate);

  // Change the active |auth_user_|. If |is_primary| is true, the active auth
  // switches to |opt_secondary_auth_|. If |is_primary| is false, the active
  // auth switches to |primary_auth_|.
  void SwapActiveAuthBetweenPrimaryAndSecondary(bool is_primary);

  // Called when an authentication check is complete.
  void OnAuthenticate(bool auth_success);

  // Tries to lookup the stored state for |user|. Returns an unowned pointer
  // that is invalidated whenver |users_| changes.
  UserState* FindStateForUser(const AccountId& user);

  // Updates the auth methods for |to_update| and |to_hide|, if passed.
  // |to_hide| will be set to LoginAuthUserView::AUTH_NONE. At minimum,
  // |to_update| will show a password prompt.
  void LayoutAuth(LoginAuthUserView* to_update,
                  LoginAuthUserView* opt_to_hide,
                  bool animate);

  // Make the user at |user_index| the auth user. We pass in the index because
  // the actual user may change.
  void SwapToAuthUser(int user_index);

  // Called after the auth user change has taken place.
  void OnAuthUserChanged();

  // Shows the correct (cached) easy unlock icon for the given auth user.
  void UpdateEasyUnlockIconForUser(const AccountId& user);

  // Get the LoginAuthUserView of the current auth user.
  LoginAuthUserView* CurrentAuthUserView();

  // Opens an error bubble to indicate authentication failure.
  void ShowAuthErrorMessage();

  // Called when the easy unlock icon is hovered.
  void OnEasyUnlockIconHovered();
  // Called when the easy unlock icon is tapped.
  void OnEasyUnlockIconTapped();

  // Returns keyboard controller for the view. Returns nullptr if keyboard is
  // not activated, view has not been added to the widget yet or keyboard is not
  // displayed in this window.
  keyboard::KeyboardController* GetKeyboardController() const;

  // Helper method to allocate a LoginAuthUserView instance.
  LoginAuthUserView* AllocateLoginAuthUserView(
      const mojom::LoginUserInfoPtr& user,
      bool is_primary);

  // Returns the authentication view for |user| if |user| is one of the active
  // authentication views. If |require_auth_active| is true then the view must
  // also be actively displaying auth.
  LoginAuthUserView* TryToFindAuthUser(const AccountId& user,
                                       bool require_auth_active);

  // Returns scrollable view with initialized size and rows for all |users|.
  ScrollableUsersListView* BuildScrollableUsersListView(
      const std::vector<mojom::LoginUserInfoPtr>& users,
      LoginDisplayStyle display_style);

  std::vector<UserState> users_;

  LoginDataDispatcher* const data_dispatcher_;  // Unowned.
  std::unique_ptr<LoginDetachableBaseModel> detachable_base_model_;

  LoginAuthUserView* primary_auth_ = nullptr;
  LoginAuthUserView* opt_secondary_auth_ = nullptr;
  ScrollableUsersListView* users_list_ = nullptr;

  // View that contains the note action button and the dev channel info labels,
  // placed on the top right corner of the screen without affecting layout of
  // other views.
  views::View* top_header_ = nullptr;

  // View for launching a note taking action handler from the lock screen.
  NoteActionLaunchButton* note_action_ = nullptr;

  // View for showing the version, enterprise and bluetooth info in dev and
  // canary channels.
  views::View* dev_channel_info_ = nullptr;

  // Contains authentication user and the additional user views.
  NonAccessibleView* main_view_ = nullptr;
  // Layout used for |main_view_|. Pointer owned by the View base class.
  views::BoxLayout* main_layout_ = nullptr;

  // Actions that should be executed when rotation changes. A full layout pass
  // is performed after all actions are executed.
  std::vector<OnRotate> rotation_actions_;

  ScopedObserver<display::Screen, display::DisplayObserver> display_observer_;
  ScopedSessionObserver session_observer_;
  ScopedObserver<keyboard::KeyboardController,
                 keyboard::KeyboardControllerObserver>
      keyboard_observer_;

  // Bubbles for displaying authentication error.
  std::unique_ptr<LoginBubble> auth_error_bubble_;

  // Bubble for displaying error when the user's detachable base changes.
  std::unique_ptr<LoginBubble> detachable_base_error_bubble_;

  std::unique_ptr<LoginBubble> tooltip_bubble_;

  int unlock_attempt_ = 0;

  // Whether a lock screen app is currently active (i.e. lock screen note action
  // state is reported as kActive by the data dispatcher).
  bool lock_screen_apps_active_ = false;

  DISALLOW_COPY_AND_ASSIGN(LockContentsView);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCK_CONTENTS_VIEW_H_
