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

DisplayErrorDialog* g_instance = NULL;

}  // namespace

// static
void  DisplayErrorDialog::ShowDialog() {
  if (g_instance) {
    DCHECK(g_instance->GetWidget());
    g_instance->GetWidget()->StackAtTop();
    g_instance->GetWidget()->Activate();
    return;
  }

  gfx::Screen* screen = Shell::GetScreen();
  const gfx::Display& target_display =
      (screen->GetNumDisplays() > 1) ?
      ScreenAsh::GetSecondaryDisplay() : screen->GetPrimaryDisplay();

  g_instance = new DisplayErrorDialog();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = g_instance;
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
}

DisplayErrorDialog::DisplayErrorDialog() {
  Shell::GetInstance()->display_controller()->AddObserver(this);
  label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_ON_MIRRORING));
  AddChildView(label_);

  label_->SetMultiLine(true);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->set_border(views::Border::CreateEmptyBorder(
      kDialogMessageMarginWidthPixel,
      kDialogMessageMarginWidthPixel,
      kDialogMessageMarginWidthPixel,
      kDialogMessageMarginWidthPixel));
  label_->SizeToFit(kDialogMessageWidthPixel);
}

DisplayErrorDialog::~DisplayErrorDialog() {
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
  g_instance = NULL;
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

// static
DisplayErrorDialog* DisplayErrorDialog::GetInstanceForTest() {
  return g_instance;
}

}  // namespace internal
}  // namespace ash
