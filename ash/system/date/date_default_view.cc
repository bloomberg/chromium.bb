// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/date_default_view.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/system/date/date_view.h"
#include "ash/system/tray/special_popup_row.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_header_button.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace {

const int kPaddingVertical = 19;

}  // namespace

namespace ash {

DateDefaultView::DateDefaultView(ash::user::LoginStatus login)
    : help_(NULL),
      shutdown_(NULL),
      lock_(NULL),
      date_view_(NULL) {
  SetLayoutManager(new views::FillLayout);

  date_view_ = new tray::DateView();
  date_view_->SetBorder(views::Border::CreateEmptyBorder(
      kPaddingVertical, ash::kTrayPopupPaddingHorizontal, 0, 0));
  SpecialPopupRow* view = new SpecialPopupRow();
  view->SetContent(date_view_);
  AddChildView(view);

  if (login == ash::user::LOGGED_IN_LOCKED ||
      login == ash::user::LOGGED_IN_NONE)
    return;

  date_view_->SetAction(TrayDate::SHOW_DATE_SETTINGS);

  help_ = new TrayPopupHeaderButton(this,
                                    IDR_AURA_UBER_TRAY_HELP,
                                    IDR_AURA_UBER_TRAY_HELP,
                                    IDR_AURA_UBER_TRAY_HELP_HOVER,
                                    IDR_AURA_UBER_TRAY_HELP_HOVER,
                                    IDS_ASH_STATUS_TRAY_HELP);
  help_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_HELP));
  view->AddButton(help_);

#if !defined(OS_WIN)
  if (login != ash::user::LOGGED_IN_LOCKED &&
      login != ash::user::LOGGED_IN_RETAIL_MODE) {
    shutdown_ = new TrayPopupHeaderButton(this,
                                          IDR_AURA_UBER_TRAY_SHUTDOWN,
                                          IDR_AURA_UBER_TRAY_SHUTDOWN,
                                          IDR_AURA_UBER_TRAY_SHUTDOWN_HOVER,
                                          IDR_AURA_UBER_TRAY_SHUTDOWN_HOVER,
                                          IDS_ASH_STATUS_TRAY_SHUTDOWN);
    shutdown_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SHUTDOWN));
    view->AddButton(shutdown_);
  }

  if (ash::Shell::GetInstance()->session_state_delegate()->CanLockScreen()) {
    lock_ = new TrayPopupHeaderButton(this,
                                      IDR_AURA_UBER_TRAY_LOCKSCREEN,
                                      IDR_AURA_UBER_TRAY_LOCKSCREEN,
                                      IDR_AURA_UBER_TRAY_LOCKSCREEN_HOVER,
                                      IDR_AURA_UBER_TRAY_LOCKSCREEN_HOVER,
                                      IDS_ASH_STATUS_TRAY_LOCK);
    lock_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOCK));
    view->AddButton(lock_);
  }
#endif  // !defined(OS_WIN)
}

DateDefaultView::~DateDefaultView() {
}

views::View* DateDefaultView::GetHelpButtonView() {
  return help_;
}

tray::DateView* DateDefaultView::GetDateView() {
  return date_view_;
}

const tray::DateView* DateDefaultView::GetDateView() const {
  return date_view_;
}

void DateDefaultView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  ash::Shell* shell = ash::Shell::GetInstance();
  ash::SystemTrayDelegate* tray_delegate = shell->system_tray_delegate();
  if (sender == help_) {
    shell->metrics()->RecordUserMetricsAction(ash::UMA_TRAY_HELP);
    tray_delegate->ShowHelp();
  } else if (sender == shutdown_) {
    shell->metrics()->RecordUserMetricsAction(ash::UMA_TRAY_SHUT_DOWN);
    tray_delegate->ShutDown();
  } else if (sender == lock_) {
    shell->metrics()->RecordUserMetricsAction(ash::UMA_TRAY_LOCK_SCREEN);
    tray_delegate->RequestLockScreen();
  } else {
    NOTREACHED();
  }
}

}  // namespace ash
