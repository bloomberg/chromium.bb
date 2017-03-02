// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/user/tray_user.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/system/user/rounded_image_view.h"
#include "ash/common/system/user/user_view.h"
#include "ash/common/wm_shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_info.h"
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
    : SystemTrayItem(system_tray, UMA_USER),
      user_index_(index),
      user_(nullptr),
      layout_view_(nullptr),
      avatar_(nullptr),
      label_(nullptr) {
  WmShell::Get()->system_tray_notifier()->AddUserObserver(this);
}

TrayUser::~TrayUser() {
  WmShell::Get()->system_tray_notifier()->RemoveUserObserver(this);
}

TrayUser::TestState TrayUser::GetStateForTest() const {
  if (!user_)
    return HIDDEN;
  return user_->GetStateForTest();
}

gfx::Size TrayUser::GetLayoutSizeForTest() const {
  return layout_view_ ? layout_view_->size() : gfx::Size();
}

gfx::Rect TrayUser::GetUserPanelBoundsInScreenForTest() const {
  DCHECK(user_);
  return user_->GetBoundsInScreenOfUserButtonForTest();
}

void TrayUser::UpdateAfterLoginStatusChangeForTest(LoginStatus status) {
  UpdateAfterLoginStatusChange(status);
}

views::View* TrayUser::CreateTrayView(LoginStatus status) {
  CHECK(layout_view_ == nullptr);

  layout_view_ = new views::View;
  UpdateAfterLoginStatusChange(status);
  return layout_view_;
}

views::View* TrayUser::CreateDefaultView(LoginStatus status) {
  if (status == LoginStatus::NOT_LOGGED_IN)
    return nullptr;
  const SessionStateDelegate* session_state_delegate =
      WmShell::Get()->GetSessionStateDelegate();

  // If the screen is locked or a system modal dialog box is shown, show only
  // the currently active user.
  if (user_index_ && (session_state_delegate->IsUserSessionBlocked() ||
                      WmShell::Get()->IsSystemModalWindowOpen()))
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

void TrayUser::UpdateAfterLoginStatusChange(LoginStatus status) {
  // Only the active user is represented in the tray.
  if (!layout_view_)
    return;
  if (user_index_ > 0)
    return;
  bool need_label = false;
  bool need_avatar = false;
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
  if (delegate->IsUserSupervised())
    need_label = true;
  switch (status) {
    case LoginStatus::LOCKED:
    case LoginStatus::USER:
    case LoginStatus::OWNER:
    case LoginStatus::PUBLIC:
      need_avatar = true;
      break;
    case LoginStatus::SUPERVISED:
      need_avatar = true;
      need_label = true;
      break;
    case LoginStatus::GUEST:
      need_label = true;
      break;
    case LoginStatus::KIOSK_APP:
    case LoginStatus::ARC_KIOSK_APP:
    case LoginStatus::NOT_LOGGED_IN:
      break;
  }

  if ((need_avatar != (avatar_ != nullptr)) ||
      (need_label != (label_ != nullptr))) {
    delete label_;
    delete avatar_;

    if (need_label) {
      label_ = new views::Label;
      SetupLabelForTray(label_);
      layout_view_->AddChildView(label_);
    } else {
      label_ = nullptr;
    }
    if (need_avatar) {
      avatar_ = new tray::RoundedImageView(kTrayRoundedBorderRadius);
      avatar_->SetPaintToLayer();
      avatar_->layer()->SetFillsBoundsOpaquely(false);
      layout_view_->AddChildView(avatar_);
    } else {
      avatar_ = nullptr;
    }
  }

  if (delegate->IsUserSupervised()) {
    label_->SetText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SUPERVISED_LABEL));
  } else if (status == LoginStatus::GUEST) {
    label_->SetText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_GUEST_LABEL));
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
      avatar_->SetCornerRadii(0, kTrayRoundedBorderRadius,
                              kTrayRoundedBorderRadius, 0);
    }
    if (label_) {
      // If label_ hasn't figured out its size yet, do that first.
      if (label_->GetContentsBounds().height() == 0)
        label_->SizeToPreferredSize();
      int height = label_->GetContentsBounds().height();
      int vertical_pad = (kTrayItemSize - height) / 2;
      int remainder = height % 2;
      label_->SetBorder(views::CreateEmptyBorder(
          vertical_pad + remainder,
          kTrayLabelItemHorizontalPaddingBottomAlignment, vertical_pad,
          kTrayLabelItemHorizontalPaddingBottomAlignment));
    }
    layout_view_->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, 0, 0, kUserLabelToIconPadding));
  } else {
    if (avatar_) {
      avatar_->SetCornerRadii(0, 0, kTrayRoundedBorderRadius,
                              kTrayRoundedBorderRadius);
    }
    if (label_) {
      label_->SetBorder(views::CreateEmptyBorder(
          kTrayLabelItemVerticalPaddingVerticalAlignment,
          kTrayLabelItemHorizontalPaddingBottomAlignment,
          kTrayLabelItemVerticalPaddingVerticalAlignment,
          kTrayLabelItemHorizontalPaddingBottomAlignment));
    }
    layout_view_->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, kUserLabelToIconPadding));
  }
}

void TrayUser::OnUserUpdate() {
  UpdateAvatarImage(
      WmShell::Get()->system_tray_delegate()->GetUserLoginStatus());
}

void TrayUser::OnUserAddedToSession() {
  SessionStateDelegate* session_state_delegate =
      WmShell::Get()->GetSessionStateDelegate();
  // Only create views for user items which are logged in.
  if (user_index_ >= session_state_delegate->NumberOfLoggedInUsers())
    return;

  // Enforce a layout change that newly added items become visible.
  UpdateLayoutOfItem();

  // Update the user item.
  UpdateAvatarImage(
      WmShell::Get()->system_tray_delegate()->GetUserLoginStatus());
}

void TrayUser::UpdateAvatarImage(LoginStatus status) {
  SessionStateDelegate* session_state_delegate =
      WmShell::Get()->GetSessionStateDelegate();
  if (!avatar_ ||
      user_index_ >= session_state_delegate->NumberOfLoggedInUsers())
    return;

  const user_manager::UserInfo* user_info =
      session_state_delegate->GetUserInfo(user_index_);
  CHECK(user_info);
  avatar_->SetImage(user_info->GetImage(),
                    gfx::Size(kTrayItemSize, kTrayItemSize));

  // Unit tests might come here with no images for some users.
  if (avatar_->size().IsEmpty())
    avatar_->SetSize(gfx::Size(kTrayItemSize, kTrayItemSize));
}

void TrayUser::UpdateLayoutOfItem() {
  UpdateAfterShelfAlignmentChange(system_tray()->shelf_alignment());
}

}  // namespace ash
