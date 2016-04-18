// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/tray_user.h"

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/user/rounded_image_view.h"
#include "ash/system/user/user_view.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

const int kUserLabelToIconPadding = 5;

}  // namespace

namespace ash {

TrayUser::TrayUser(SystemTray* system_tray, UserIndex index)
    : SystemTrayItem(system_tray),
      user_index_(index),
      user_(nullptr),
      layout_view_(nullptr),
      avatar_(nullptr),
      label_(nullptr) {
  Shell::GetInstance()->system_tray_notifier()->AddUserObserver(this);
}

TrayUser::~TrayUser() {
  Shell::GetInstance()->system_tray_notifier()->RemoveUserObserver(this);
}

TrayUser::TestState TrayUser::GetStateForTest() const {
  if (!user_)
    return HIDDEN;
  return user_->GetStateForTest();
}

gfx::Size TrayUser::GetLayoutSizeForTest() const {
  if (!layout_view_) {
    return gfx::Size(0, 0);
  } else {
    return layout_view_->size();
  }
}

gfx::Rect TrayUser::GetUserPanelBoundsInScreenForTest() const {
  DCHECK(user_);
  return user_->GetBoundsInScreenOfUserButtonForTest();
}

void TrayUser::UpdateAfterLoginStatusChangeForTest(user::LoginStatus status) {
  UpdateAfterLoginStatusChange(status);
}

views::View* TrayUser::CreateTrayView(user::LoginStatus status) {
  CHECK(layout_view_ == nullptr);

  layout_view_ = new views::View;
  layout_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           0, 0, kUserLabelToIconPadding));
  UpdateAfterLoginStatusChange(status);
  return layout_view_;
}

views::View* TrayUser::CreateDefaultView(user::LoginStatus status) {
  if (status == user::LOGGED_IN_NONE)
    return nullptr;
  const SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();

  // If the screen is locked or a system modal dialog box is shown, show only
  // the currently active user.
  if (user_index_ && (session_state_delegate->IsUserSessionBlocked() ||
                      Shell::GetInstance()->IsSystemModalWindowOpen()))
    return nullptr;

  CHECK(user_ == nullptr);

  int logged_in_users = session_state_delegate->NumberOfLoggedInUsers();

  // Do not show more UserView's then there are logged in users.
  if (user_index_ >= logged_in_users)
    return nullptr;

  user_ = new tray::UserView(this, status, user_index_);
  return user_;
}

void TrayUser::DestroyTrayView() {
  layout_view_ = nullptr;
  avatar_ = nullptr;
  label_ = nullptr;
}

void TrayUser::DestroyDefaultView() {
  user_ = nullptr;
}

void TrayUser::UpdateAfterLoginStatusChange(user::LoginStatus status) {
  // Only the active user is represented in the tray.
  if (!layout_view_)
    return;
  if (user_index_ > 0)
    return;
  bool need_label = false;
  bool need_avatar = false;
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  if (delegate->IsUserSupervised())
    need_label =  true;
  switch (status) {
    case user::LOGGED_IN_LOCKED:
    case user::LOGGED_IN_USER:
    case user::LOGGED_IN_OWNER:
    case user::LOGGED_IN_PUBLIC:
      need_avatar = true;
      break;
    case user::LOGGED_IN_SUPERVISED:
      need_avatar = true;
      need_label = true;
      break;
    case user::LOGGED_IN_GUEST:
      need_label = true;
      break;
    case user::LOGGED_IN_KIOSK_APP:
    case user::LOGGED_IN_NONE:
      break;
  }

  if ((need_avatar != (avatar_ != nullptr)) ||
      (need_label != (label_ != nullptr))) {
    layout_view_->RemoveAllChildViews(true);
    if (need_label) {
      label_ = new views::Label;
      SetupLabelForTray(label_);
      layout_view_->AddChildView(label_);
    } else {
      label_ = nullptr;
    }
    if (need_avatar) {
      avatar_ = new tray::RoundedImageView(kTrayAvatarCornerRadius, true);
      layout_view_->AddChildView(avatar_);
    } else {
      avatar_ = nullptr;
    }
  }

  if (delegate->IsUserSupervised()) {
    label_->SetText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL));
  } else if (status == user::LOGGED_IN_GUEST) {
    label_->SetText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_GUEST_LABEL));
  }

  if (avatar_) {
    avatar_->SetCornerRadii(
        0, kTrayAvatarCornerRadius, kTrayAvatarCornerRadius, 0);
    avatar_->SetBorder(views::Border::NullBorder());
  }
  UpdateAvatarImage(status);

  // Update layout after setting label_ and avatar_ with new login status.
  UpdateLayoutOfItem();
}

void TrayUser::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  // Inactive users won't have a layout.
  if (!layout_view_)
    return;
  if (IsHorizontalAlignment(alignment)) {
    if (avatar_) {
      avatar_->SetBorder(views::Border::NullBorder());
      avatar_->SetCornerRadii(
          0, kTrayAvatarCornerRadius, kTrayAvatarCornerRadius, 0);
    }
    if (label_) {
      // If label_ hasn't figured out its size yet, do that first.
      if (label_->GetContentsBounds().height() == 0)
        label_->SizeToPreferredSize();
      int height = label_->GetContentsBounds().height();
      int vertical_pad = (kTrayItemSize - height) / 2;
      int remainder = height % 2;
      label_->SetBorder(views::Border::CreateEmptyBorder(
          vertical_pad + remainder,
          kTrayLabelItemHorizontalPaddingBottomAlignment,
          vertical_pad,
          kTrayLabelItemHorizontalPaddingBottomAlignment));
    }
    layout_view_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal,
                             0, 0, kUserLabelToIconPadding));
  } else {
    if (avatar_) {
      avatar_->SetBorder(views::Border::NullBorder());
      avatar_->SetCornerRadii(
          0, 0, kTrayAvatarCornerRadius, kTrayAvatarCornerRadius);
    }
    if (label_) {
      label_->SetBorder(views::Border::CreateEmptyBorder(
          kTrayLabelItemVerticalPaddingVerticalAlignment,
          kTrayLabelItemHorizontalPaddingBottomAlignment,
          kTrayLabelItemVerticalPaddingVerticalAlignment,
          kTrayLabelItemHorizontalPaddingBottomAlignment));
    }
    layout_view_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical,
                             0, 0, kUserLabelToIconPadding));
  }
}

void TrayUser::OnUserUpdate() {
  UpdateAvatarImage(Shell::GetInstance()->system_tray_delegate()->
      GetUserLoginStatus());
}

void TrayUser::OnUserAddedToSession() {
  SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();
  // Only create views for user items which are logged in.
  if (user_index_ >= session_state_delegate->NumberOfLoggedInUsers())
    return;

  // Enforce a layout change that newly added items become visible.
  UpdateLayoutOfItem();

  // Update the user item.
  UpdateAvatarImage(
      Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus());
}

void TrayUser::UpdateAvatarImage(user::LoginStatus status) {
  SessionStateDelegate* session_state_delegate =
      Shell::GetInstance()->session_state_delegate();
  if (!avatar_ ||
      user_index_ >= session_state_delegate->NumberOfLoggedInUsers())
    return;

  const user_manager::UserInfo* user_info =
      session_state_delegate->GetUserInfo(user_index_);
  CHECK(user_info);
  avatar_->SetImage(user_info->GetImage(),
                    gfx::Size(kTrayAvatarSize, kTrayAvatarSize));

  // Unit tests might come here with no images for some users.
  if (avatar_->size().IsEmpty())
    avatar_->SetSize(gfx::Size(kTrayAvatarSize, kTrayAvatarSize));
}

void TrayUser::UpdateLayoutOfItem() {
  UpdateAfterShelfAlignmentChange(system_tray()->shelf_alignment());
}

}  // namespace ash
