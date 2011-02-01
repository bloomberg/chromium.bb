// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/views_login_display.h"

#include <algorithm>

#include "base/stl_util-inl.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_helper.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/ui/views/window.h"
#include "grit/chromium_strings.h"
#include "grit/theme_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

namespace {

// Max number of users we'll show. The true max is the min of this and the
// number of windows that fit on the screen.
const size_t kMaxUsers = 6;

// Minimum number of users we'll show (including Guest and New User pods).
const size_t kMinUsers = 3;

// Used to indicate no user has been selected.
const size_t kNotSelected = -1;

// Offset of cursor in first position from edit left side. It's used to position
// info bubble arrow to cursor.
const int kCursorOffset = 5;

// Checks if display names are unique. If there are duplicates, enables
// tooltips with full emails to let users distinguish their accounts.
// Otherwise, disables the tooltips.
void EnableTooltipsIfNeeded(
    const std::vector<chromeos::UserController*>& controllers) {
  std::map<std::string, int> visible_display_names;
  for (size_t i = 0; i + 1 < controllers.size(); ++i) {
    const std::string& display_name =
        controllers[i]->user().GetDisplayName();
    ++visible_display_names[display_name];
  }
  for (size_t i = 0; i < controllers.size(); ++i) {
    const std::string& display_name =
        controllers[i]->user().GetDisplayName();
    bool show_tooltip = controllers[i]->is_new_user() ||
                        controllers[i]->is_guest() ||
                        visible_display_names[display_name] > 1;
    controllers[i]->EnableNameTooltip(show_tooltip);
  }
}

}  // namespace

namespace chromeos {

ViewsLoginDisplay::ViewsLoginDisplay(LoginDisplay::Delegate* delegate,
                                     const gfx::Rect& background_bounds)
    : LoginDisplay(delegate, background_bounds),
      bubble_(NULL),
      controller_for_removal_(NULL),
      selected_view_index_(kNotSelected) {
}

ViewsLoginDisplay::~ViewsLoginDisplay() {
  ClearErrors();
  STLDeleteElements(&controllers_);
  STLDeleteElements(&invisible_controllers_);
}

////////////////////////////////////////////////////////////////////////////////
// ViewsLoginDisplay, LoginDisplay implementation:
//

void ViewsLoginDisplay::Init(const std::vector<UserManager::User>& users,
                             bool show_guest,
                             bool show_new_user) {
  size_t max_users = kMaxUsers;
  if (width() > 0) {
    size_t users_per_screen = (width() - login::kUserImageSize) /
        (UserController::kUnselectedSize + UserController::kPadding);
    max_users = std::max(kMinUsers, std::min(kMaxUsers, users_per_screen));
  }

  size_t visible_users_count = std::min(users.size(), max_users -
      static_cast<int>(show_guest) - static_cast<int>(show_new_user));
  for (size_t i = 0; i < users.size(); ++i) {
    UserController* user_controller = new UserController(this, users[i]);
    if (controllers_.size() < visible_users_count)
      controllers_.push_back(user_controller);
    else
      invisible_controllers_.push_back(user_controller);
  }
  if (show_guest)
    controllers_.push_back(new UserController(this, true));

  if (show_new_user)
    controllers_.push_back(new UserController(this, false));

  // If there's only new user pod, show the guest session link on it.
  bool show_guest_link = controllers_.size() == 1;
  for (size_t i = 0; i < controllers_.size(); ++i) {
    (controllers_[i])->Init(static_cast<int>(i),
                            static_cast<int>(controllers_.size()),
                            show_guest_link);
  }
  EnableTooltipsIfNeeded(controllers_);
}

void ViewsLoginDisplay::OnBeforeUserRemoved(const std::string& username) {
  controller_for_removal_ = controllers_[selected_view_index_];
  controllers_.erase(controllers_.begin() + selected_view_index_);
  EnableTooltipsIfNeeded(controllers_);

  // Update user count before unmapping windows, otherwise window manager won't
  // be in the right state.
  int new_size = static_cast<int>(controllers_.size());
  for (int i = 0; i < new_size; ++i)
    controllers_[i]->UpdateUserCount(i, new_size);
}

void ViewsLoginDisplay::OnUserRemoved(const std::string& username) {
  // We need to unmap entry windows, the windows will be unmapped in destructor.
  delete controller_for_removal_;
  controller_for_removal_ = NULL;

  // Nothing to insert.
  if (invisible_controllers_.empty())
    return;

  // Insert just before guest or add new user pods if any.
  int new_size = static_cast<int>(controllers_.size());
  int insert_position = new_size;
  while (insert_position > 0 &&
         (controllers_[insert_position - 1]->is_new_user() ||
          controllers_[insert_position - 1]->is_guest()))
    --insert_position;

  controllers_.insert(controllers_.begin() + insert_position,
                      invisible_controllers_[0]);
  invisible_controllers_.erase(invisible_controllers_.begin());

  // Update counts for exiting pods.
  new_size = static_cast<int>(controllers_.size());
  for (int i = 0; i < new_size; ++i) {
    if (i != insert_position)
      controllers_[i]->UpdateUserCount(i, new_size);
  }

  // And initialize new one that was invisible.
  controllers_[insert_position]->Init(insert_position, new_size, false);

  EnableTooltipsIfNeeded(controllers_);
}

void ViewsLoginDisplay::SetUIEnabled(bool is_enabled) {
  // Send message to WM to enable/disable click on windows.
  WmIpc::Message message(WM_IPC_MESSAGE_WM_SET_LOGIN_STATE);
  message.set_param(0, is_enabled);
  WmIpc::instance()->SendMessage(message);

  if (is_enabled)
    controllers_[selected_view_index_]->ClearAndEnablePassword();
}

void ViewsLoginDisplay::ShowError(int error_msg_id,
                                  int login_attempts,
                                  HelpAppLauncher::HelpTopic help_topic_id) {
  ClearErrors();
  string16 error_text;
  help_topic_id_ = help_topic_id;

  // GetStringF fails on debug build if there's no replacement in the string.
  if (error_msg_id == IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED) {
    error_text = l10n_util::GetStringFUTF16(
        error_msg_id, l10n_util::GetStringUTF16(IDS_PRODUCT_OS_NAME));
  } else {
    error_text = l10n_util::GetStringUTF16(error_msg_id);
  }

  gfx::Rect bounds =
      controllers_[selected_view_index_]->GetMainInputScreenBounds();
  BubbleBorder::ArrowLocation arrow;
  if (controllers_[selected_view_index_]->is_new_user()) {
    arrow = BubbleBorder::LEFT_TOP;
  } else {
    // Point info bubble arrow to cursor position (approximately).
    bounds.set_width(kCursorOffset * 2);
    arrow = BubbleBorder::BOTTOM_LEFT;
  }

  string16 help_link;
  if (error_msg_id == IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED) {
    help_link = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  } else if (login_attempts > 1) {
    help_link = l10n_util::GetStringUTF16(IDS_CANT_ACCESS_ACCOUNT_BUTTON);
  }

  bubble_ = MessageBubble::Show(
      controllers_[selected_view_index_]->controls_window(),
      bounds,
      arrow,
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
      UTF16ToWide(error_text),
      UTF16ToWide(help_link),
      this);
  WizardAccessibilityHelper::GetInstance()->MaybeSpeak(
      UTF16ToUTF8(error_text).c_str(), false, false);
}

////////////////////////////////////////////////////////////////////////////////
// ViewsLoginDisplay, UserController::Delegate implementation:
//

void ViewsLoginDisplay::CreateAccount() {
  delegate()->CreateAccount();
}

void ViewsLoginDisplay::Login(UserController* source,
                              const string16& password) {
  delegate()->Login(source->user().email(), UTF16ToUTF8(password));
}

void ViewsLoginDisplay::LoginAsGuest() {
  delegate()->LoginAsGuest();
}

void ViewsLoginDisplay::ClearErrors() {
  // bubble_ will be set to NULL in callback.
  if (bubble_)
    bubble_->Close();
}

void ViewsLoginDisplay::OnUserSelected(UserController* source) {
  std::vector<UserController*>::const_iterator i =
      std::find(controllers_.begin(), controllers_.end(), source);
  DCHECK(i != controllers_.end());
  size_t new_selected_index = i - controllers_.begin();
  if (new_selected_index != selected_view_index_ &&
      selected_view_index_ != kNotSelected) {
    controllers_[selected_view_index_]->ClearAndEnableFields();
    controllers_[new_selected_index]->ClearAndEnableFields();
    delegate()->OnUserSelected(source->user().email());
  }
  selected_view_index_ = new_selected_index;
}

void ViewsLoginDisplay::RemoveUser(UserController* source) {
  ClearErrors();
  delegate()->RemoveUser(source->user().email());
}

void ViewsLoginDisplay::SelectUser(int index) {
  if (index >= 0 && index < static_cast<int>(controllers_.size()) &&
      index != static_cast<int>(selected_view_index_)) {
    WmIpc::Message message(WM_IPC_MESSAGE_WM_SELECT_LOGIN_USER);
    message.set_param(0, index);
    WmIpc::instance()->SendMessage(message);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ViewsLoginDisplay, views::MessageBubbleDelegate implementation:
//

void ViewsLoginDisplay::OnHelpLinkActivated() {
  if (!parent_window())
    return;
  if (!help_app_.get())
    help_app_.reset(new HelpAppLauncher(parent_window()));
  help_app_->ShowHelpTopic(help_topic_id_);
}

}  // namespace chromeos
