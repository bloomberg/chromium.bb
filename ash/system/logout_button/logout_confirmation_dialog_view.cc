// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/logout_button/logout_confirmation_dialog_view.h"

#include "ash/shell.h"
#include "ash/system/logout_button/logout_button_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "base/location.h"
#include "grit/ash_strings.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

const int kCountdownUpdateIntervalMs = 1000;  // 1 second.

inline int Round(double x) {
    return static_cast<int>(x + 0.5);
}

}  // namespace

LogoutConfirmationDialogView::LogoutConfirmationDialogView(
    LogoutButtonTray* owner, Delegate* delegate) : owner_(owner),
                                                   delegate_(delegate) {
  text_label_ = new views::Label;
  text_label_->SetBorder(views::Border::CreateEmptyBorder(
      0, kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingHorizontal));
  text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label_->SetMultiLine(true);

  SetLayoutManager(new views::FillLayout());

  AddChildView(text_label_);
}

LogoutConfirmationDialogView::~LogoutConfirmationDialogView() {
}

bool LogoutConfirmationDialogView::Accept() {
  LogoutCurrentUser();
  return true;
}

ui::ModalType LogoutConfirmationDialogView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 LogoutConfirmationDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ASH_LOGOUT_CONFIRMATION_TITLE);
}

base::string16 LogoutConfirmationDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_ASH_LOGOUT_CONFIRMATION_BUTTON);
  return views::DialogDelegateView::GetDialogButtonLabel(button);
}

void LogoutConfirmationDialogView::OnClosed() {
  owner_->ReleaseConfirmationDialog();
  owner_ = NULL;
  timer_.Stop();
  // Nullify the delegate to prevent future activities of the dialog.
  delegate_ = NULL;
}

void LogoutConfirmationDialogView::DeleteDelegate() {
  if (owner_)
    owner_->ReleaseConfirmationDialog();
  delete this;
}

void LogoutConfirmationDialogView::Show(base::TimeDelta duration) {
  if (!delegate_)
    return;
  countdown_start_time_ = delegate_->GetCurrentTime();
  duration_ = duration;

  UpdateCountdown();

  delegate_->ShowDialog(this);

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(kCountdownUpdateIntervalMs),
               this,
               &LogoutConfirmationDialogView::UpdateCountdown);
}

void LogoutConfirmationDialogView::UpdateDialogDuration(
    base::TimeDelta duration) {
  duration_ = duration;
  UpdateCountdown();
}

void LogoutConfirmationDialogView::LogoutCurrentUser() {
  if (!delegate_)
    return;
  delegate_->LogoutCurrentUser();
}

void LogoutConfirmationDialogView::UpdateCountdown() {
  if (!delegate_)
    return;
  const base::TimeDelta time_remaining = countdown_start_time_ +
      duration_ - delegate_->GetCurrentTime();
  // Round the remaining time to nearest second, and use this value for
  // the countdown display and actual enforcement.
  int seconds_remaining = Round(time_remaining.InSecondsF());
  if (seconds_remaining > 0) {
    text_label_->SetText(l10n_util::GetStringFUTF16(
        IDS_ASH_LOGOUT_CONFIRMATION_WARNING,
        ui::TimeFormat::Simple(
            ui::TimeFormat::FORMAT_DURATION, ui::TimeFormat::LENGTH_LONG,
            base::TimeDelta::FromSeconds(seconds_remaining))));
  } else {
    text_label_->SetText(l10n_util::GetStringUTF16(
        IDS_ASH_LOGOUT_CONFIRMATION_WARNING_NOW));
    timer_.Stop();
    LogoutCurrentUser();
  }
}

}  // namespace internal
}  // namespace ash
