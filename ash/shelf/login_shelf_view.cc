// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_view.h"

#include "ash/ash_constants.h"
#include "ash/login/lock_screen_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shutdown_controller.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wm/lock_state_controller.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

using session_manager::SessionState;

namespace ash {

LoginShelfView::LoginShelfView()
    : tray_action_observer_(this), shutdown_controller_observer_(this) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal));

  // TODO(wzang): Add the correct text and image for each type.
  AddButton(kShutdown, base::ASCIIToUTF16("Shut down(FIXME)"),
            gfx::ImageSkia());
  AddButton(kRestart, base::ASCIIToUTF16("Restart(FIXME)"), gfx::ImageSkia());
  AddButton(kSignOut, base::ASCIIToUTF16("Sign out(FIXME)"), gfx::ImageSkia());
  AddButton(kCloseNote, base::ASCIIToUTF16("Unlock(FIXME)"), gfx::ImageSkia());
  AddButton(kCancel, base::ASCIIToUTF16("Cancel(FIXME)"), gfx::ImageSkia());

  // Adds observers for states that affect the visiblity of different buttons.
  tray_action_observer_.Add(Shell::Get()->tray_action());
  shutdown_controller_observer_.Add(Shell::Get()->shutdown_controller());

  UpdateUi();
}

LoginShelfView::~LoginShelfView() = default;

void LoginShelfView::UpdateAfterSessionStateChange(SessionState state) {
  UpdateUi();
}

void LoginShelfView::ButtonPressed(views::Button* sender,
                                   const ui::Event& event) {
  switch (sender->id()) {
    case kShutdown:
    case kRestart:
      // |ShutdownController| will further distinguish the two cases based on
      // shutdown policy.
      Shell::Get()->lock_state_controller()->RequestShutdown(
          ShutdownReason::LOGIN_SHUT_DOWN_BUTTON);
      break;
    case kSignOut:
      base::RecordAction(base::UserMetricsAction("ScreenLocker_Signout"));
      Shell::Get()->shell_delegate()->Exit();
      break;
    case kCloseNote:
      Shell::Get()->tray_action()->CloseLockScreenNote(
          mojom::CloseLockScreenNoteReason::kUnlockButtonPressed);
      break;
    case kCancel:
      Shell::Get()->lock_screen_controller()->CancelAddUser();
      break;
  }
}

void LoginShelfView::OnLockScreenNoteStateChanged(
    mojom::TrayActionState state) {
  UpdateUi();
}

void LoginShelfView::OnShutdownPolicyChanged(bool reboot_on_shutdown) {
  UpdateUi();
}

void LoginShelfView::AddButton(ButtonId button_id,
                               base::string16 text,
                               gfx::ImageSkia image) {
  views::LabelButton* button = new views::LabelButton(this, text);
  AddChildView(button);

  button->set_id(button_id);
  button->SetAccessibleName(text);
  button->SetImage(views::Button::STATE_NORMAL, image);
  button->SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      ash::kFocusBorderColor, ash::kFocusBorderThickness, gfx::InsetsF()));
  button->SetFocusBehavior(FocusBehavior::ALWAYS);
  button->SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
  button->set_ink_drop_base_color(kShelfInkDropBaseColor);
  button->set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);
}

void LoginShelfView::UpdateUi() {
  SessionState session_state =
      Shell::Get()->session_controller()->GetSessionState();
  if (session_state == SessionState::ACTIVE) {
    // The entire view was set invisible. The buttons are also set invisible
    // to avoid affecting calculation of the shelf size.
    for (int i = 0; i < child_count(); ++i)
      child_at(i)->SetVisible(false);
    return;
  }
  bool show_reboot = Shell::Get()->shutdown_controller()->reboot_on_shutdown();
  mojom::TrayActionState tray_action_state =
      Shell::Get()->tray_action()->GetLockScreenNoteState();
  bool is_lock_screen_note_in_foreground =
      tray_action_state == mojom::TrayActionState::kActive ||
      tray_action_state == mojom::TrayActionState::kLaunching;
  // The following should be kept in sync with |updateUI_| in md_header_bar.js.
  GetViewByID(kShutdown)->SetVisible(!show_reboot &&
                                     !is_lock_screen_note_in_foreground);
  GetViewByID(kRestart)->SetVisible(show_reboot &&
                                    !is_lock_screen_note_in_foreground);
  GetViewByID(kSignOut)->SetVisible(session_state == SessionState::LOCKED &&
                                    !is_lock_screen_note_in_foreground);
  GetViewByID(kCloseNote)
      ->SetVisible(session_state == SessionState::LOCKED &&
                   is_lock_screen_note_in_foreground);
  GetViewByID(kCancel)->SetVisible(session_state ==
                                   SessionState::LOGIN_SECONDARY);
  Layout();
}

}  // namespace ash
