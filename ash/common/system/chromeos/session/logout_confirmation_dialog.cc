// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/session/logout_confirmation_dialog.h"

#include "ash/common/system/chromeos/session/logout_confirmation_controller.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/location.h"
#include "base/time/tick_clock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const int kCountdownUpdateIntervalMs = 1000;  // 1 second.

const int kHalfSecondInMs = 500;  // Half a second.

}  // namespace

LogoutConfirmationDialog::LogoutConfirmationDialog(
    LogoutConfirmationController* controller,
    base::TimeTicks logout_time)
    : controller_(controller), logout_time_(logout_time) {
  SetLayoutManager(new views::FillLayout());

  label_ = new views::Label;
  label_->SetBorder(views::CreateEmptyBorder(0, kTrayPopupPaddingHorizontal, 0,
                                             kTrayPopupPaddingHorizontal));
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
  AddChildView(label_);

  UpdateLabel();

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params =
      GetDialogWidgetInitParams(this, nullptr, nullptr, gfx::Rect());
  WmShell::Get()
      ->GetPrimaryRootWindow()
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          widget, kShellWindowId_SystemModalContainer, &params);
  widget->Init(params);
  widget->Show();

  update_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kCountdownUpdateIntervalMs),
      this, &LogoutConfirmationDialog::UpdateLabel);
}

LogoutConfirmationDialog::~LogoutConfirmationDialog() {}

void LogoutConfirmationDialog::Update(base::TimeTicks logout_time) {
  logout_time_ = logout_time;
  UpdateLabel();
}

void LogoutConfirmationDialog::ControllerGone() {
  controller_ = nullptr;
  GetWidget()->Close();
}

bool LogoutConfirmationDialog::Accept() {
  logout_time_ = controller_->clock()->NowTicks();
  UpdateLabel();
  controller_->OnLogoutConfirmed();
  return true;
}

ui::ModalType LogoutConfirmationDialog::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 LogoutConfirmationDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ASH_LOGOUT_CONFIRMATION_TITLE);
}

base::string16 LogoutConfirmationDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_ASH_LOGOUT_CONFIRMATION_BUTTON);
  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

void LogoutConfirmationDialog::WindowClosing() {
  update_timer_.Stop();
  if (controller_)
    controller_->OnDialogClosed();
}

void LogoutConfirmationDialog::UpdateLabel() {
  const base::TimeDelta time_remaining =
      logout_time_ - controller_->clock()->NowTicks();
  if (time_remaining >= base::TimeDelta::FromMilliseconds(kHalfSecondInMs)) {
    label_->SetText(l10n_util::GetStringFUTF16(
        IDS_ASH_LOGOUT_CONFIRMATION_WARNING,
        ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                                 ui::TimeFormat::LENGTH_LONG, 10,
                                 time_remaining)));
  } else {
    label_->SetText(
        l10n_util::GetStringUTF16(IDS_ASH_LOGOUT_CONFIRMATION_WARNING_NOW));
    update_timer_.Stop();
  }
}

}  // namespace ash
