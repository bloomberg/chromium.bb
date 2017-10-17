// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_debug_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/login/lock_screen_controller.h"
#include "ash/login/ui/layout_util.h"
#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/login/ui/non_accessible_view.h"
#include "ash/shell.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

constexpr const char* kDebugUserNames[] = {
    "Angelina Johnson", "Marcus Cohen", "Chris Wallace",
    "Debbie Craig",     "Stella Wong",  "Stephanie Wade",
};

// Additional state for a user that the debug UI needs to reference.
struct UserMetadata {
  explicit UserMetadata(const mojom::UserInfoPtr& user_info)
      : account_id(user_info->account_id) {}

  AccountId account_id;
  bool enable_pin = false;
  bool enable_click_to_unlock = false;
  mojom::EasyUnlockIconId easy_unlock_id = mojom::EasyUnlockIconId::NONE;

  views::View* view = nullptr;
};

}  // namespace

// Applies a series of user-defined transformations to a |LoginDataDispatcher|
// instance; this is used for debugging and development. The debug overlay uses
// this class to change what data is exposed to the UI.
class LockDebugView::DebugDataDispatcherTransformer
    : public LoginDataDispatcher::Observer {
 public:
  DebugDataDispatcherTransformer(
      mojom::TrayActionState initial_lock_screen_note_state,
      LoginDataDispatcher* dispatcher)
      : root_dispatcher_(dispatcher),
        lock_screen_note_state_(initial_lock_screen_note_state) {
    root_dispatcher_->AddObserver(this);
  }
  ~DebugDataDispatcherTransformer() override {
    root_dispatcher_->RemoveObserver(this);
  }

  LoginDataDispatcher* debug_dispatcher() { return &debug_dispatcher_; }

  // Changes the number of displayed users to |count|.
  void SetUserCount(int count) {
    DCHECK(!root_users_.empty());

    count = std::max(count, 1);

    // Trim any extra debug users.
    if (debug_users_.size() > size_t{count})
      debug_users_.erase(debug_users_.begin() + count, debug_users_.end());

    // Build |users|, add any new users to |debug_users|.
    std::vector<mojom::LoginUserInfoPtr> users;
    for (size_t i = 0; i < size_t{count}; ++i) {
      const mojom::LoginUserInfoPtr& root_user =
          root_users_[i % root_users_.size()];
      users.push_back(root_user->Clone());
      if (i >= root_users_.size()) {
        users[i]->basic_user_info->account_id = AccountId::FromUserEmailGaiaId(
            users[i]->basic_user_info->account_id.GetUserEmail() +
                std::to_string(i),
            users[i]->basic_user_info->account_id.GetGaiaId() +
                std::to_string(i));
      }
      if (i >= debug_users_.size())
        debug_users_.push_back(UserMetadata(users[i]->basic_user_info));
    }

    // Set debug user names. Useful for the stub user, which does not have a
    // name set.
    for (size_t i = 0; i < users.size(); ++i)
      users[i]->basic_user_info->display_name =
          kDebugUserNames[i % arraysize(kDebugUserNames)];

    // User notification resets PIN state.
    for (UserMetadata& user : debug_users_)
      user.enable_pin = false;

    debug_dispatcher_.NotifyUsers(users);
  }

  // Activates or deactivates PIN for the user at |user_index|.
  void TogglePinStateForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];
    debug_user->enable_pin = !debug_user->enable_pin;
    debug_dispatcher_.SetPinEnabledForUser(debug_user->account_id,
                                           debug_user->enable_pin);
  }

  // Enables click to auth for the user at |user_index|.
  void CycleEasyUnlockForUserIndex(size_t user_index) {
    DCHECK(user_index >= 0 && user_index < debug_users_.size());
    UserMetadata* debug_user = &debug_users_[user_index];

    // EasyUnlockIconId state transition.
    auto get_next_id = [](mojom::EasyUnlockIconId id) {
      switch (id) {
        case mojom::EasyUnlockIconId::NONE:
          return mojom::EasyUnlockIconId::SPINNER;
        case mojom::EasyUnlockIconId::SPINNER:
          return mojom::EasyUnlockIconId::LOCKED;
        case mojom::EasyUnlockIconId::LOCKED:
          return mojom::EasyUnlockIconId::LOCKED_TO_BE_ACTIVATED;
        case mojom::EasyUnlockIconId::LOCKED_TO_BE_ACTIVATED:
          return mojom::EasyUnlockIconId::LOCKED_WITH_PROXIMITY_HINT;
        case mojom::EasyUnlockIconId::LOCKED_WITH_PROXIMITY_HINT:
          return mojom::EasyUnlockIconId::HARDLOCKED;
        case mojom::EasyUnlockIconId::HARDLOCKED:
          return mojom::EasyUnlockIconId::UNLOCKED;
        case mojom::EasyUnlockIconId::UNLOCKED:
          return mojom::EasyUnlockIconId::NONE;
      }
      return mojom::EasyUnlockIconId::NONE;
    };
    debug_user->easy_unlock_id = get_next_id(debug_user->easy_unlock_id);

    // Enable/disable click to unlock.
    debug_user->enable_click_to_unlock =
        debug_user->easy_unlock_id == mojom::EasyUnlockIconId::UNLOCKED;

    // Prepare icon that we will show.
    auto icon = mojom::EasyUnlockIconOptions::New();
    icon->icon = debug_user->easy_unlock_id;
    if (icon->icon == mojom::EasyUnlockIconId::SPINNER) {
      icon->aria_label = base::ASCIIToUTF16("Icon is spinning");
    } else if (icon->icon == mojom::EasyUnlockIconId::LOCKED ||
               icon->icon == mojom::EasyUnlockIconId::LOCKED_TO_BE_ACTIVATED) {
      icon->autoshow_tooltip = true;
      icon->tooltip = base::ASCIIToUTF16(
          "This is a long message to trigger overflow. This should show up "
          "automatically. icon_id=" +
          std::to_string(static_cast<int>(icon->icon)));
    } else {
      icon->tooltip =
          base::ASCIIToUTF16("This should not show up automatically.");
    }

    // Show icon and enable/disable click to unlock.
    debug_dispatcher_.ShowEasyUnlockIcon(debug_user->account_id, icon);
    debug_dispatcher_.SetClickToUnlockEnabledForUser(
        debug_user->account_id, debug_user->enable_click_to_unlock);
  }

  void ToggleLockScreenNoteButton() {
    if (lock_screen_note_state_ == mojom::TrayActionState::kAvailable) {
      lock_screen_note_state_ = mojom::TrayActionState::kNotAvailable;
    } else {
      lock_screen_note_state_ = mojom::TrayActionState::kAvailable;
    }

    debug_dispatcher_.SetLockScreenNoteState(lock_screen_note_state_);
  }

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(
      const std::vector<mojom::LoginUserInfoPtr>& users) override {
    // Update root_users_ to new source data.
    root_users_.clear();
    for (auto& user : users)
      root_users_.push_back(user->Clone());

    // Rebuild debug users using new source data.
    SetUserCount(debug_users_.size());
  }
  void OnPinEnabledForUserChanged(const AccountId& user,
                                  bool enabled) override {
    // Forward notification only if the user is currently being shown.
    for (size_t i = 0u; i < debug_users_.size(); ++i) {
      if (debug_users_[i].account_id == user) {
        debug_users_[i].enable_pin = enabled;
        debug_dispatcher_.SetPinEnabledForUser(user, enabled);
        break;
      }
    }
  }
  void OnClickToUnlockEnabledForUserChanged(const AccountId& user,
                                            bool enabled) override {
    // Forward notification only if the user is currently being shown.
    for (size_t i = 0u; i < debug_users_.size(); ++i) {
      if (debug_users_[i].account_id == user) {
        debug_users_[i].enable_click_to_unlock = enabled;
        debug_dispatcher_.SetClickToUnlockEnabledForUser(user, enabled);
        break;
      }
    }
  }
  void OnLockScreenNoteStateChanged(mojom::TrayActionState state) override {
    lock_screen_note_state_ = state;
    debug_dispatcher_.SetLockScreenNoteState(state);
  }
  void OnShowEasyUnlockIcon(
      const AccountId& user,
      const mojom::EasyUnlockIconOptionsPtr& icon) override {
    debug_dispatcher_.ShowEasyUnlockIcon(user, icon);
  }

 private:
  // The debug overlay UI takes ground-truth data from |root_dispatcher_|,
  // applies a series of transformations to it, and exposes it to the UI via
  // |debug_dispatcher_|.
  LoginDataDispatcher* root_dispatcher_;  // Unowned.
  LoginDataDispatcher debug_dispatcher_;

  // Original set of users from |root_dispatcher_|.
  std::vector<mojom::LoginUserInfoPtr> root_users_;

  // Metadata for users that the UI is displaying.
  std::vector<UserMetadata> debug_users_;

  // The current lock screen note action state.
  mojom::TrayActionState lock_screen_note_state_;

  DISALLOW_COPY_AND_ASSIGN(DebugDataDispatcherTransformer);
};

LockDebugView::LockDebugView(mojom::TrayActionState initial_note_action_state,
                             LoginDataDispatcher* data_dispatcher)
    : debug_data_dispatcher_(std::make_unique<DebugDataDispatcherTransformer>(
          initial_note_action_state,
          data_dispatcher)) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal));

  lock_ = new LockContentsView(initial_note_action_state,
                               debug_data_dispatcher_->debug_dispatcher());
  AddChildView(lock_);

  debug_row_ = new NonAccessibleView();
  debug_row_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal));
  AddChildView(debug_row_);

  per_user_action_column_ = new NonAccessibleView();
  per_user_action_column_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical));
  debug_row_->AddChildView(per_user_action_column_);

  auto* margin = new NonAccessibleView();
  margin->SetPreferredSize(gfx::Size(10, 10));
  debug_row_->AddChildView(margin);

  toggle_blur_ = AddButton("Blur");
  toggle_note_action_ = AddButton("Toggle note action");
  toggle_caps_lock_ = AddButton("Toggle caps lock");
  add_user_ = AddButton("Add user");
  remove_user_ = AddButton("Remove user");
  toggle_auth_ = AddButton("Force fail auth");

  RebuildDebugUserColumn();
}

LockDebugView::~LockDebugView() = default;

void LockDebugView::Layout() {
  views::View::Layout();
  lock_->SetBoundsRect(GetLocalBounds());
  debug_row_->SetPosition(gfx::Point());
  debug_row_->SizeToPreferredSize();
}

void LockDebugView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  // Enable or disable wallpaper blur.
  if (sender == toggle_blur_) {
    LockScreen::Get()->ToggleBlurForDebug();
    return;
  }

  // Enable or disable note action.
  if (sender == toggle_note_action_) {
    debug_data_dispatcher_->ToggleLockScreenNoteButton();
    return;
  }

  // Enable or disable caps lock.
  if (sender == toggle_caps_lock_) {
    chromeos::input_method::ImeKeyboard* keyboard =
        chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
    keyboard->SetCapsLockEnabled(!keyboard->CapsLockIsEnabled());
    return;
  }

  // Add or remove a user.
  if (sender == add_user_ || sender == remove_user_) {
    if (sender == add_user_)
      ++num_users_;
    else if (sender == remove_user_)
      --num_users_;
    if (num_users_ < 1u)
      num_users_ = 1u;
    debug_data_dispatcher_->SetUserCount(num_users_);
    RebuildDebugUserColumn();
    Layout();
    return;
  }

  // Enable/disable auth. This is useful for testing auth failure scenarios on
  // Linux Desktop builds, where the cryptohome dbus stub accepts all passwords
  // as valid.
  if (sender == toggle_auth_) {
    force_fail_auth_ = !force_fail_auth_;
    toggle_auth_->SetText(base::ASCIIToUTF16(
        force_fail_auth_ ? "Allow auth" : "Force fail auth"));
    Shell::Get()
        ->lock_screen_controller()
        ->set_force_fail_auth_for_debug_overlay(force_fail_auth_);
    return;
  }

  // Enable or disable PIN.
  for (size_t i = 0u; i < per_user_action_column_toggle_pin_.size(); ++i) {
    if (per_user_action_column_toggle_pin_[i] == sender)
      debug_data_dispatcher_->TogglePinStateForUserIndex(i);
  }

  // Cycle easy unlock.
  for (size_t i = 0u;
       i < per_user_action_column_cycle_easy_unlock_state_.size(); ++i) {
    if (per_user_action_column_cycle_easy_unlock_state_[i] == sender)
      debug_data_dispatcher_->CycleEasyUnlockForUserIndex(i);
  }
}

void LockDebugView::RebuildDebugUserColumn() {
  per_user_action_column_->RemoveAllChildViews(true /*delete_children*/);
  per_user_action_column_toggle_pin_.clear();
  per_user_action_column_cycle_easy_unlock_state_.clear();

  for (size_t i = 0u; i < num_users_; ++i) {
    auto* row = new NonAccessibleView();
    row->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal));

    views::View* toggle_pin =
        AddButton("Toggle PIN", false /*add_to_debug_row*/);
    per_user_action_column_toggle_pin_.push_back(toggle_pin);
    row->AddChildView(toggle_pin);

    views::View* toggle_click_auth =
        AddButton("Cycle easy unlock", false /*add_to_debug_row*/);
    per_user_action_column_cycle_easy_unlock_state_.push_back(
        toggle_click_auth);
    row->AddChildView(toggle_click_auth);

    per_user_action_column_->AddChildView(row);
  }
}

views::MdTextButton* LockDebugView::AddButton(const std::string& text,
                                              bool add_to_debug_row) {
  // Creates a button with |text| that cannot be focused.
  auto* button = views::MdTextButton::Create(this, base::ASCIIToUTF16(text));
  button->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  if (add_to_debug_row)
    debug_row_->AddChildView(
        login_layout_util::WrapViewForPreferredSize(button));
  return button;
}

}  // namespace ash
