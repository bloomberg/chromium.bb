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
#include "ash/wm/lock_state_controller.h"
#include "base/i18n/rtl.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace {

// The ISO-639 code for the Hebrew locale. The help icon asset is a '?' which is
// not mirrored in this locale.
const char kHebrewLocale[] = "he";

const int kPaddingVertical = 19;

}  // namespace

namespace ash {

DateDefaultView::DateDefaultView(ash::user::LoginStatus login)
    : help_button_(NULL),
      shutdown_button_(NULL),
      lock_button_(NULL),
      date_view_(NULL),
      weak_factory_(this) {
  SetLayoutManager(new views::FillLayout);

  date_view_ = new tray::DateView();
  date_view_->SetBorder(views::Border::CreateEmptyBorder(
      kPaddingVertical, ash::kTrayPopupPaddingHorizontal, 0, 0));
  SpecialPopupRow* view = new SpecialPopupRow();
  view->SetContent(date_view_);
  AddChildView(view);

  bool userAddingRunning = ash::Shell::GetInstance()
                               ->session_state_delegate()
                               ->IsInSecondaryLoginScreen();

  if (login == user::LOGGED_IN_LOCKED ||
      login == user::LOGGED_IN_NONE || userAddingRunning)
    return;

  date_view_->SetAction(TrayDate::SHOW_DATE_SETTINGS);

  help_button_ = new TrayPopupHeaderButton(
      this, IDR_AURA_UBER_TRAY_HELP, IDR_AURA_UBER_TRAY_HELP,
      IDR_AURA_UBER_TRAY_HELP_HOVER, IDR_AURA_UBER_TRAY_HELP_HOVER,
      IDS_ASH_STATUS_TRAY_HELP);
  if (base::i18n::IsRTL() &&
      base::i18n::GetConfiguredLocale() == kHebrewLocale) {
    // The asset for the help button is a question mark '?'. Normally this asset
    // is flipped in RTL locales, however Hebrew uses the LTR '?'. So the
    // flipping must be disabled. (crbug.com/475237)
    help_button_->EnableCanvasFlippingForRTLUI(false);
  }
  help_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_HELP));
  view->AddButton(help_button_);

#if !defined(OS_WIN)
  if (login != ash::user::LOGGED_IN_LOCKED) {
    shutdown_button_ = new TrayPopupHeaderButton(
        this, IDR_AURA_UBER_TRAY_SHUTDOWN, IDR_AURA_UBER_TRAY_SHUTDOWN,
        IDR_AURA_UBER_TRAY_SHUTDOWN_HOVER, IDR_AURA_UBER_TRAY_SHUTDOWN_HOVER,
        IDS_ASH_STATUS_TRAY_SHUTDOWN);
    shutdown_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SHUTDOWN));
    view->AddButton(shutdown_button_);
  }

  if (ash::Shell::GetInstance()->session_state_delegate()->CanLockScreen()) {
    lock_button_ = new TrayPopupHeaderButton(
        this, IDR_AURA_UBER_TRAY_LOCKSCREEN, IDR_AURA_UBER_TRAY_LOCKSCREEN,
        IDR_AURA_UBER_TRAY_LOCKSCREEN_HOVER,
        IDR_AURA_UBER_TRAY_LOCKSCREEN_HOVER, IDS_ASH_STATUS_TRAY_LOCK);
    lock_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOCK));
    view->AddButton(lock_button_);
  }
  SystemTrayDelegate* system_tray_delegate =
      Shell::GetInstance()->system_tray_delegate();
  system_tray_delegate->AddShutdownPolicyObserver(this);
  system_tray_delegate->ShouldRebootOnShutdown(base::Bind(
      &DateDefaultView::OnShutdownPolicyChanged, weak_factory_.GetWeakPtr()));
#endif  // !defined(OS_WIN)
}

DateDefaultView::~DateDefaultView() {
  // We need the check as on shell destruction, the delegate is destroyed first.
  SystemTrayDelegate* system_tray_delegate =
      Shell::GetInstance()->system_tray_delegate();
  if (system_tray_delegate)
    system_tray_delegate->RemoveShutdownPolicyObserver(this);
}

views::View* DateDefaultView::GetHelpButtonView() {
  return help_button_;
}

const views::View* DateDefaultView::GetShutdownButtonViewForTest() const {
  return shutdown_button_;
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
  if (sender == help_button_) {
    shell->metrics()->RecordUserMetricsAction(ash::UMA_TRAY_HELP);
    tray_delegate->ShowHelp();
  } else if (sender == shutdown_button_) {
    shell->metrics()->RecordUserMetricsAction(ash::UMA_TRAY_SHUT_DOWN);
    ash::Shell::GetInstance()->lock_state_controller()->RequestShutdown();
  } else if (sender == lock_button_) {
    shell->metrics()->RecordUserMetricsAction(ash::UMA_TRAY_LOCK_SCREEN);
    tray_delegate->RequestLockScreen();
  } else {
    NOTREACHED();
  }
}

void DateDefaultView::OnShutdownPolicyChanged(bool reboot_on_shutdown) {
  if (!shutdown_button_)
    return;

  shutdown_button_->SetTooltipText(l10n_util::GetStringUTF16(
      reboot_on_shutdown ? IDS_ASH_STATUS_TRAY_REBOOT
                         : IDS_ASH_STATUS_TRAY_SHUTDOWN));
}

}  // namespace ash
