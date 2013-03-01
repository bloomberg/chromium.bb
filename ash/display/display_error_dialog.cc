// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_error_dialog.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "grit/ash_strings.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {
namespace {

// The width of the area to show the error message.
const int kDialogMessageWidthPixel = 300;

// The margin width from the error message to the edge of the dialog.
const int kDialogMessageMarginWidthPixel = 5;

}  // namespace

// static
DisplayErrorDialog* DisplayErrorDialog::ShowDialog(
    chromeos::OutputState new_state) {
  gfx::Screen* screen = Shell::GetScreen();
  const gfx::Display& target_display =
      (screen->GetNumDisplays() > 1) ?
      ScreenAsh::GetSecondaryDisplay() : screen->GetPrimaryDisplay();

  DisplayErrorDialog* dialog = new DisplayErrorDialog(new_state);
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = dialog;
  // Makes |widget| belong to the target display.  Size and location are
  // fixed by CenterWindow() below.
  params.bounds = target_display.bounds();
  DisplayController* display_controller =
      Shell::GetInstance()->display_controller();
  params.context =
      display_controller->GetRootWindowForDisplayId(target_display.id());
  params.keep_on_top = true;
  widget->Init(params);

  widget->GetNativeView()->SetName("DisplayErrorDialog");
  widget->CenterWindow(widget->GetRootView()->GetPreferredSize());
  widget->Show();
  return dialog;
}

void DisplayErrorDialog::UpdateMessageForState(
    chromeos::OutputState new_state) {
  int message_id = -1;
  switch (new_state) {
    case chromeos::STATE_DUAL_MIRROR:
      message_id = IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING;
      break;
    case chromeos::STATE_DUAL_EXTENDED:
      message_id = IDS_ASH_DISPLAY_FAILURE_ON_EXTENDED;
      break;
    default:
      // The error dialog would appear only for mirroring or extended.
      // It's quite unlikely to happen with other status (single-display /
      // invalid / unknown status), but set an unknown error message
      // instead of NOTREACHED() just in case for safety.
      LOG(ERROR) << "Unexpected failure for new state: " << new_state;
      message_id = IDS_ASH_DISPLAY_FAILURE_UNKNOWN;
      break;
  }
  label_->SetText(l10n_util::GetStringUTF16(message_id));
  label_->SizeToFit(kDialogMessageWidthPixel);
  Layout();
  SchedulePaint();
}

DisplayErrorDialog::DisplayErrorDialog(chromeos::OutputState new_state) {
  Shell::GetInstance()->display_controller()->AddObserver(this);
  label_ = new views::Label();
  AddChildView(label_);

  label_->SetMultiLine(true);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->set_border(views::Border::CreateEmptyBorder(
      kDialogMessageMarginWidthPixel,
      kDialogMessageMarginWidthPixel,
      kDialogMessageMarginWidthPixel,
      kDialogMessageMarginWidthPixel));

  UpdateMessageForState(new_state);
}

DisplayErrorDialog::~DisplayErrorDialog() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

int DisplayErrorDialog::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

ui::ModalType DisplayErrorDialog::GetModalType() const {
  return ui::MODAL_TYPE_NONE;
}

gfx::Size DisplayErrorDialog::GetPreferredSize() {
  return label_->GetPreferredSize();
}

void DisplayErrorDialog::OnDisplayConfigurationChanging() {
  GetWidget()->Close();
}

DisplayErrorObserver::DisplayErrorObserver()
    : dialog_(NULL) {
}

DisplayErrorObserver::~DisplayErrorObserver() {
  DCHECK(!dialog_);
}

void DisplayErrorObserver::OnDisplayModeChangeFailed(
    chromeos::OutputState new_state) {
  if (dialog_) {
    DCHECK(dialog_->GetWidget());
    dialog_->UpdateMessageForState(new_state);
    dialog_->GetWidget()->StackAtTop();
    dialog_->GetWidget()->Activate();
  } else {
    dialog_ = DisplayErrorDialog::ShowDialog(new_state);
    dialog_->GetWidget()->AddObserver(this);
  }
}

void DisplayErrorObserver::OnWidgetClosing(views::Widget* widget) {
  DCHECK(dialog_);
  DCHECK_EQ(dialog_->GetWidget(), widget);
  widget->RemoveObserver(this);
  dialog_ = NULL;
}

}  // namespace internal
}  // namespace ash
