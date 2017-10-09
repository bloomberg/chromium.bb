// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/login_shelf_view.h"

#include <memory>

#include "ash/ash_constants.h"
#include "ash/focus_cycler.h"
#include "ash/login/lock_screen_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shutdown_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wm/lock_state_controller.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"

using session_manager::SessionState;

namespace ash {
namespace {

// Spacing between the button image and label.
constexpr int kImageLabelSpacingDp = 8;

// The width of the four margins of each button.
constexpr int kButtonMarginDp = 13;

// The color of the button image and label.
constexpr SkColor kButtonColor = SK_ColorWHITE;

class LoginShelfButton : public views::LabelButton {
 public:
  LoginShelfButton(views::ButtonListener* listener,
                   const base::string16& text,
                   const gfx::ImageSkia& image)
      : LabelButton(listener, text) {
    SetAccessibleName(text);
    SetImage(views::Button::STATE_NORMAL, image);
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetFocusPainter(views::Painter::CreateSolidFocusPainter(
        kFocusBorderColor, kFocusBorderThickness, gfx::InsetsF()));
    SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
    set_ink_drop_base_color(kShelfInkDropBaseColor);
    set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);

    SetImageLabelSpacing(kImageLabelSpacingDp);
    SetTextColor(views::Button::STATE_NORMAL, kButtonColor);
    SetTextColor(views::Button::STATE_HOVERED, kButtonColor);
    SetTextColor(views::Button::STATE_PRESSED, kButtonColor);
    label()->SetFontList(views::Label::GetDefaultFontList().Derive(
        1, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
  }

  ~LoginShelfButton() override = default;

  // views::View:
  gfx::Insets GetInsets() const override {
    return gfx::Insets(kButtonMarginDp);
  }

  // views::InkDropHostView:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop =
        std::make_unique<views::InkDropImpl>(this, size());
    ink_drop->SetShowHighlightOnHover(false);
    ink_drop->SetShowHighlightOnFocus(false);
    return std::move(ink_drop);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginShelfButton);
};

}  // namespace

LoginShelfView::LoginShelfView()
    : tray_action_observer_(this), shutdown_controller_observer_(this) {
  // We reuse the focusable state on this view as a signal that focus should
  // switch to the lock screen or status area. This view should otherwise not
  // be focusable.
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal));

  auto add_button = [this](ButtonId id, int text_resource_id,
                           const gfx::VectorIcon& icon) {
    const base::string16 text = l10n_util::GetStringUTF16(text_resource_id);
    gfx::ImageSkia image = CreateVectorIcon(icon, kButtonColor);
    LoginShelfButton* button = new LoginShelfButton(this, text, image);
    button->set_id(id);
    AddChildView(button);
  };
  add_button(kShutdown, IDS_ASH_SHELF_SHUTDOWN_BUTTON,
             kShelfShutdownButtonIcon);
  add_button(kRestart, IDS_ASH_SHELF_RESTART_BUTTON, kShelfShutdownButtonIcon);
  add_button(kSignOut, IDS_ASH_SHELF_SIGN_OUT_BUTTON, kShelfSignOutButtonIcon);
  add_button(kCloseNote, IDS_ASH_SHELF_UNLOCK_BUTTON, kShelfUnlockButtonIcon);
  add_button(kCancel, IDS_ASH_SHELF_CANCEL_BUTTON, kShelfCancelButtonIcon);

  // Adds observers for states that affect the visiblity of different buttons.
  tray_action_observer_.Add(Shell::Get()->tray_action());
  shutdown_controller_observer_.Add(Shell::Get()->shutdown_controller());

  UpdateUi();
}

LoginShelfView::~LoginShelfView() = default;

void LoginShelfView::UpdateAfterSessionStateChange(SessionState state) {
  UpdateUi();
}

void LoginShelfView::OnFocus() {
  LOG(WARNING) << "LoginShelfView was focused, but this should never happen. "
                  "Forwarded focus to shelf widget with an unknown direction.";
  Shell::Get()->focus_cycler()->FocusWidget(
      Shelf::ForWindow(GetWidget()->GetNativeWindow())->shelf_widget());
}

void LoginShelfView::AboutToRequestFocusFromTabTraversal(bool reverse) {
  if (reverse) {
    // Focus should leave the system tray.
    Shell::Get()->system_tray_notifier()->NotifyFocusOut(reverse);
  } else {
    // Focus goes to status area.
    Shelf::ForWindow(GetWidget()->GetNativeWindow())
        ->GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(
        Shelf::ForWindow(GetWidget()->GetNativeWindow())
            ->GetStatusAreaWidget());
  }
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
      Shell::Get()->session_controller()->RequestSignOut();
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
