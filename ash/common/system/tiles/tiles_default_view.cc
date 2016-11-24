// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tiles/tiles_default_view.h"

#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shutdown_controller.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#endif

namespace {

// The ISO-639 code for the Hebrew locale. The help icon asset is a '?' which is
// not mirrored in this locale.
const char kHebrewLocale[] = "he";

}  // namespace

namespace ash {

TilesDefaultView::TilesDefaultView(SystemTrayItem* owner, LoginStatus login)
    : owner_(owner),
      login_(login),
      settings_button_(nullptr),
      help_button_(nullptr),
      lock_button_(nullptr),
      power_button_(nullptr) {}

TilesDefaultView::~TilesDefaultView() {}

void TilesDefaultView::Init() {
  WmShell* shell = WmShell::Get();
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 4, 0, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(box_layout);

  // Show the buttons in this row as disabled if the user is at the login
  // screen, lock screen, or in a secondary account flow. The exception is
  // |power_button_| which is always shown as enabled.
  const bool disable_buttons = !TrayPopupUtils::CanOpenWebUISettings(login_);

  settings_button_ = new SystemMenuButton(
      this, TrayPopupInkDropStyle::HOST_CENTERED, kSystemMenuSettingsIcon,
      IDS_ASH_STATUS_TRAY_SETTINGS);
  if (disable_buttons || !shell->system_tray_delegate()->ShouldShowSettings())
    settings_button_->SetEnabled(false);
  AddChildView(settings_button_);
  AddChildView(TrayPopupUtils::CreateVerticalSeparator());

  help_button_ =
      new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED,
                           kSystemMenuHelpIcon, IDS_ASH_STATUS_TRAY_HELP);
  if (base::i18n::IsRTL() &&
      base::i18n::GetConfiguredLocale() == kHebrewLocale) {
    // The asset for the help button is a question mark '?'. Normally this asset
    // is flipped in RTL locales, however Hebrew uses the LTR '?'. So the
    // flipping must be disabled. (crbug.com/475237)
    help_button_->EnableCanvasFlippingForRTLUI(false);
  }
  if (disable_buttons)
    help_button_->SetEnabled(false);
  AddChildView(help_button_);
  AddChildView(TrayPopupUtils::CreateVerticalSeparator());

#if !defined(OS_WIN)
  lock_button_ =
      new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED,
                           kSystemMenuLockIcon, IDS_ASH_STATUS_TRAY_LOCK);
  if (disable_buttons || !shell->GetSessionStateDelegate()->CanLockScreen())
    lock_button_->SetEnabled(false);

  AddChildView(lock_button_);
  AddChildView(TrayPopupUtils::CreateVerticalSeparator());

  power_button_ =
      new SystemMenuButton(this, TrayPopupInkDropStyle::HOST_CENTERED,
                           kSystemMenuPowerIcon, IDS_ASH_STATUS_TRAY_SHUTDOWN);
  AddChildView(power_button_);
  // This object is recreated every time the menu opens. Don't bother updating
  // the tooltip if the shutdown policy changes while the menu is open.
  bool reboot = WmShell::Get()->shutdown_controller()->reboot_on_shutdown();
  power_button_->SetTooltipText(l10n_util::GetStringUTF16(
      reboot ? IDS_ASH_STATUS_TRAY_REBOOT : IDS_ASH_STATUS_TRAY_SHUTDOWN));
#endif  // !defined(OS_WIN)
}

void TilesDefaultView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  DCHECK(sender);
  WmShell* shell = WmShell::Get();
  if (sender == settings_button_) {
    shell->RecordUserMetricsAction(UMA_TRAY_SETTINGS);
    shell->system_tray_controller()->ShowSettings();
  } else if (sender == help_button_) {
    shell->RecordUserMetricsAction(UMA_TRAY_HELP);
    shell->system_tray_controller()->ShowHelp();
  } else if (sender == lock_button_) {
    shell->RecordUserMetricsAction(UMA_TRAY_LOCK_SCREEN);
#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Get()
        ->GetSessionManagerClient()
        ->RequestLockScreen();
#endif
  } else if (sender == power_button_) {
    shell->RecordUserMetricsAction(UMA_TRAY_SHUT_DOWN);
    shell->RequestShutdown();
  }

  owner_->system_tray()->CloseSystemBubble();
}

views::View* TilesDefaultView::GetHelpButtonView() const {
  return help_button_;
}

const views::CustomButton* TilesDefaultView::GetShutdownButtonViewForTest()
    const {
  return power_button_;
}

}  // namespace ash
