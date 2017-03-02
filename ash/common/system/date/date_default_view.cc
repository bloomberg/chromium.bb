// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/date/date_default_view.h"

#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shutdown_controller.h"
#include "ash/common/system/date/date_view.h"
#include "ash/common/system/tray/special_popup_row.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/lock_state_controller.h"
#include "base/i18n/rtl.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
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

DateDefaultView::DateDefaultView(SystemTrayItem* owner, LoginStatus login)
    : help_button_(nullptr),
      shutdown_button_(nullptr),
      lock_button_(nullptr),
      date_view_(nullptr) {
  SetLayoutManager(new views::FillLayout);

  date_view_ = new tray::DateView(owner);
  date_view_->SetBorder(views::CreateEmptyBorder(
      kPaddingVertical, ash::kTrayPopupPaddingHorizontal, 0, 0));
  SpecialPopupRow* view = new SpecialPopupRow();
  view->SetContent(date_view_);
  AddChildView(view);

  WmShell* shell = WmShell::Get();
  const bool adding_user =
      shell->GetSessionStateDelegate()->IsInSecondaryLoginScreen();

  if (login == LoginStatus::LOCKED || login == LoginStatus::NOT_LOGGED_IN ||
      adding_user)
    return;

  date_view_->SetAction(tray::DateView::DateAction::SHOW_DATE_SETTINGS);

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
  view->AddViewToRowNonMd(help_button_, true);

  if (login != LoginStatus::LOCKED) {
    shutdown_button_ = new TrayPopupHeaderButton(
        this, IDR_AURA_UBER_TRAY_SHUTDOWN, IDR_AURA_UBER_TRAY_SHUTDOWN,
        IDR_AURA_UBER_TRAY_SHUTDOWN_HOVER, IDR_AURA_UBER_TRAY_SHUTDOWN_HOVER,
        IDS_ASH_STATUS_TRAY_SHUTDOWN);
    shutdown_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SHUTDOWN));
    view->AddViewToRowNonMd(shutdown_button_, true);
    // This object is recreated every time the menu opens. Don't bother updating
    // the tooltip if the shutdown policy changes while the menu is open.
    bool reboot = WmShell::Get()->shutdown_controller()->reboot_on_shutdown();
    shutdown_button_->SetTooltipText(l10n_util::GetStringUTF16(
        reboot ? IDS_ASH_STATUS_TRAY_REBOOT : IDS_ASH_STATUS_TRAY_SHUTDOWN));
  }

  if (shell->GetSessionStateDelegate()->CanLockScreen()) {
    lock_button_ = new TrayPopupHeaderButton(
        this, IDR_AURA_UBER_TRAY_LOCKSCREEN, IDR_AURA_UBER_TRAY_LOCKSCREEN,
        IDR_AURA_UBER_TRAY_LOCKSCREEN_HOVER,
        IDR_AURA_UBER_TRAY_LOCKSCREEN_HOVER, IDS_ASH_STATUS_TRAY_LOCK);
    lock_button_->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOCK));
    view->AddViewToRowNonMd(lock_button_, true);
  }
}

DateDefaultView::~DateDefaultView() {}

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
  WmShell* shell = WmShell::Get();
  if (sender == help_button_) {
    shell->RecordUserMetricsAction(UMA_TRAY_HELP);
    shell->system_tray_controller()->ShowHelp();
  } else if (sender == shutdown_button_) {
    shell->RecordUserMetricsAction(UMA_TRAY_SHUT_DOWN);
    Shell::GetInstance()->lock_state_controller()->RequestShutdown();
  } else if (sender == lock_button_) {
    shell->RecordUserMetricsAction(UMA_TRAY_LOCK_SCREEN);
    chromeos::DBusThreadManager::Get()
        ->GetSessionManagerClient()
        ->RequestLockScreen();
  } else {
    NOTREACHED();
  }
  date_view_->CloseSystemBubble();
}

}  // namespace ash
