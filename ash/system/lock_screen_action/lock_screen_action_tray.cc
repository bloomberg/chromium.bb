// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/lock_screen_action/lock_screen_action_tray.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_container.h"
#include "ash/tray_action/tray_action.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chromeos/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

bool IsLockScreenActionTrayEnabled() {
  // The lock screen action entry point will be move from system tray to the
  // lock screen UI - for current incarnation of lock screen UI, this has
  // already been done, so the tray action button is not needed in this case.
  // For views base lock screen (used when show-md-login is set), this is not
  // the case.
  // TODO(tbarzic): Replace LockScreenActionTray item with a button in the lock
  //     screen UI for views base lock screen implementation.
  //     http://crbug.com/746596
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kShowMdLogin);
}

}  // namespace

LockScreenActionTray::LockScreenActionTray(Shelf* shelf)
    : TrayBackgroundView(shelf),
      session_observer_(this),
      tray_action_observer_(this) {
  SetVisible(false);

  if (!IsLockScreenActionTrayEnabled())
    return;

  SetInkDropMode(InkDropMode::ON);
  new_note_action_view_ = new views::ImageView();
  new_note_action_view_->SetImage(
      CreateVectorIcon(kTrayActionNewLockScreenNoteIcon, kShelfIconColor));
  new_note_action_view_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_CREATE_NOTE_ACTION));
  new_note_action_view_->SetPreferredSize(
      gfx::Size(kTrayItemSize, kTrayItemSize));
  tray_container()->AddChildView(new_note_action_view_);
}

LockScreenActionTray::~LockScreenActionTray() {}

void LockScreenActionTray::Initialize() {
  TrayBackgroundView::Initialize();

  if (!IsLockScreenActionTrayEnabled())
    return;

  session_observer_.Add(Shell::Get()->session_controller());

  TrayAction* controller = Shell::Get()->tray_action();
  tray_action_observer_.Add(controller);

  OnLockScreenNoteStateChanged(controller->GetLockScreenNoteState());
}

bool LockScreenActionTray::PerformAction(const ui::Event& event) {
  Shell::Get()->tray_action()->RequestNewLockScreenNote(
      mojom::LockScreenNoteOrigin::kTrayAction);
  return true;
}

base::string16 LockScreenActionTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_CREATE_NOTE_ACTION);
}

bool LockScreenActionTray::ShouldBlockShelfAutoHide() const {
  return false;
}

void LockScreenActionTray::HideBubbleWithView(
    const views::TrayBubbleView* bubble_view) {}

void LockScreenActionTray::ClickedOutsideBubble() {}

void LockScreenActionTray::OnLockStateChanged(bool locked) {
  UpdateNewNoteIcon();
}

void LockScreenActionTray::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {
  new_note_state_ = state;

  UpdateNewNoteIcon();
}

bool LockScreenActionTray::IsStateVisible() const {
  return IsLockScreenActionTrayEnabled() &&
         Shell::Get()->session_controller()->IsScreenLocked() &&
         (new_note_state_ == mojom::TrayActionState::kAvailable ||
          new_note_state_ == mojom::TrayActionState::kLaunching);
}

void LockScreenActionTray::UpdateNewNoteIcon() {
  SetIsActive(new_note_state_ == mojom::TrayActionState::kLaunching ||
              new_note_state_ == mojom::TrayActionState::kActive ||
              new_note_state_ == mojom::TrayActionState::kBackground);

  if (!IsStateVisible()) {
    SetVisible(false);
    return;
  }

  new_note_action_view_->SetEnabled(new_note_state_ !=
                                    mojom::TrayActionState::kLaunching);

  SetVisible(true);
}

const gfx::ImageSkia& LockScreenActionTray::GetImageForTesting() const {
  return new_note_action_view_->GetImage();
}

}  // namespace ash
