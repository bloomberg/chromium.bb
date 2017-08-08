// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_debug_view.h"

#include <algorithm>

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace {

const char* kDebugUserNames[] = {
    "Angelina Johnson", "Marcus Cohen", "Chris Wallace",
    "Debbie Craig",     "Stella Wong",  "Stephanie Wade",
};

// Additional state for a user that the debug UI needs to reference.
struct UserMetadata {
  explicit UserMetadata(const ash::mojom::UserInfoPtr& user_info)
      : account_id(user_info->account_id) {}

  AccountId account_id;
  bool enable_pin = false;
  views::View* view = nullptr;
};

// Wraps |view| so it is sized to its preferred dimensions.
views::View* WrapViewForPreferredSize(views::View* view) {
  auto* proxy = new views::View();
  proxy->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical));
  proxy->AddChildView(view);
  return proxy;
}

}  // namespace

// Applies a series of user-defined transformations to a |LoginDataDispatcher|
// instance; this is used for debugging and development. The debug overlay uses
// this class to change what data is exposed to the UI.
class LockDebugView::DebugDataDispatcherTransformer
    : public LoginDataDispatcher::Observer {
 public:
  explicit DebugDataDispatcherTransformer(LoginDataDispatcher* dispatcher)
      : root_dispatcher_(dispatcher) {
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
    std::vector<ash::mojom::UserInfoPtr> users;
    for (size_t i = 0; i < size_t{count}; ++i) {
      const ash::mojom::UserInfoPtr& root_user =
          root_users_[i % root_users_.size()];
      users.push_back(root_user->Clone());
      if (i >= root_users_.size()) {
        users[i]->account_id = AccountId::FromUserEmailGaiaId(
            users[i]->account_id.GetUserEmail() + std::to_string(i),
            users[i]->account_id.GetGaiaId() + std::to_string(i));
      }
      if (i >= debug_users_.size())
        debug_users_.push_back(UserMetadata(users[i]));
    }

    // Set debug user names. Useful for the stub user, which does not have a
    // name set.
    for (size_t i = 0; i < users.size(); ++i)
      users[i]->display_name = kDebugUserNames[i % arraysize(kDebugUserNames)];

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

  // LoginDataDispatcher::Observer:
  void OnUsersChanged(
      const std::vector<ash::mojom::UserInfoPtr>& users) override {
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

 private:
  // The debug overlay UI takes ground-truth data from |root_dispatcher_|,
  // applies a series of transformations to it, and exposes it to the UI via
  // |debug_dispatcher_|.
  LoginDataDispatcher* root_dispatcher_;  // Unowned.
  LoginDataDispatcher debug_dispatcher_;

  // Original set of users from |root_dispatcher_|.
  std::vector<ash::mojom::UserInfoPtr> root_users_;

  // Metadata for users that the UI is displaying.
  std::vector<UserMetadata> debug_users_;

  DISALLOW_COPY_AND_ASSIGN(DebugDataDispatcherTransformer);
};

LockDebugView::LockDebugView(LoginDataDispatcher* data_dispatcher)
    : debug_data_dispatcher_(
          base::MakeUnique<DebugDataDispatcherTransformer>(data_dispatcher)) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal));

  lock_ = new LockContentsView(debug_data_dispatcher_->debug_dispatcher());
  AddChildView(lock_);

  debug_ = new views::View();
  debug_->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal));
  AddChildView(debug_);

  toggle_blur_ = views::MdTextButton::Create(this, base::ASCIIToUTF16("Blur"));
  debug_->AddChildView(WrapViewForPreferredSize(toggle_blur_));

  add_user_ = views::MdTextButton::Create(this, base::ASCIIToUTF16("Add"));
  debug_->AddChildView(WrapViewForPreferredSize(add_user_));

  remove_user_ =
      views::MdTextButton::Create(this, base::ASCIIToUTF16("Remove"));
  debug_->AddChildView(WrapViewForPreferredSize(remove_user_));

  user_column_ = new views::View();
  user_column_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical));
  debug_->AddChildView(user_column_);

  RebuildDebugUserColumn();
}

LockDebugView::~LockDebugView() = default;

void LockDebugView::Layout() {
  views::View::Layout();
  lock_->SetBoundsRect(GetLocalBounds());
  debug_->SetPosition(gfx::Point());
  debug_->SizeToPreferredSize();
}

void LockDebugView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  // Enable or disable wallpaper blur.
  if (sender == toggle_blur_) {
    LockScreen::Get()->ToggleBlurForDebug();
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

  // Enable or disable PIN.
  for (size_t i = 0u; i < user_column_entries_toggle_pin_.size(); ++i) {
    if (user_column_entries_toggle_pin_[i] == sender)
      debug_data_dispatcher_->TogglePinStateForUserIndex(i);
  }
}

void LockDebugView::RebuildDebugUserColumn() {
  user_column_->RemoveAllChildViews(true /*delete_children*/);
  user_column_entries_toggle_pin_.clear();

  for (size_t i = 0u; i < num_users_; ++i) {
    views::View* toggle_pin =
        views::MdTextButton::Create(this, base::ASCIIToUTF16("Toggle PIN"));
    user_column_entries_toggle_pin_.push_back(toggle_pin);
    user_column_->AddChildView(toggle_pin);
  }
}

}  // namespace ash