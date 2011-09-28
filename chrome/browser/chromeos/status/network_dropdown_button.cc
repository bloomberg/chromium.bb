// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_dropdown_button.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton

NetworkDropdownButton::NetworkDropdownButton(bool is_browser_mode,
                                             gfx::NativeWindow parent_window,
                                             bool should_show_options)
    : DropDownButton(NULL,
                     UTF16ToWide(l10n_util::GetStringUTF16(
                         IDS_STATUSBAR_NO_NETWORKS_MESSAGE)),
                     this,
                     true),
      parent_window_(parent_window),
      should_show_options_(should_show_options) {
  network_menu_.reset(new NetworkMenu(this, is_browser_mode));
  network_icon_.reset(
      new NetworkMenuIcon(this, NetworkMenuIcon::DROPDOWN_MODE));
  CrosLibrary::Get()->GetNetworkLibrary()->AddNetworkManagerObserver(this);
  // The initial state will be updated on Refresh.
  // See network_selection_view.cc.
}

NetworkDropdownButton::~NetworkDropdownButton() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
}

void NetworkDropdownButton::Refresh() {
  OnNetworkManagerChanged(CrosLibrary::Get()->GetNetworkLibrary());
}

void NetworkDropdownButton::SetFirstLevelMenuWidth(int width) {
  network_menu_->set_min_width(width);
}

void NetworkDropdownButton::CancelMenu() {
  network_menu_->CancelMenu();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, NetworkMenu::Delegate implementation:

views::MenuButton* NetworkDropdownButton::GetMenuButton() {
  return this;
}

gfx::NativeWindow NetworkDropdownButton::GetNativeWindow() const {
  return parent_window_;
}

void NetworkDropdownButton::OpenButtonOptions() {
  if (proxy_settings_dialog_.get() == NULL) {
    proxy_settings_dialog_.reset(new ProxySettingsDialog(this,
                                                         GetNativeWindow()));
  }
  proxy_settings_dialog_->Show();
}

bool NetworkDropdownButton::ShouldOpenButtonOptions() const {
  return should_show_options_;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, views::ViewMenuDelegate implementation:

void NetworkDropdownButton::RunMenu(views::View* source, const gfx::Point& pt) {
  network_menu_->RunMenu(source);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, overridden from views::View.

void NetworkDropdownButton::OnLocaleChanged() {
  // Proxy settings dialog contains localized strings.
  proxy_settings_dialog_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, LoginHtmlDialog::Delegate implementation:

void NetworkDropdownButton::OnDialogClosed() {
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, NetworkLibrary::NetworkManagerObserver implementation:

void NetworkDropdownButton::OnNetworkManagerChanged(NetworkLibrary* cros) {
  // This gets called on initialization, so any changes should be reflected
  // in CrosMock::SetNetworkLibraryStatusAreaExpectations().
  SetNetworkIconAndText();
  SchedulePaint();
  network_menu_->UpdateMenu();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, NetworkMenuIcon::Delegate implementation:
void NetworkDropdownButton::NetworkMenuIconChanged() {
  const SkBitmap bitmap = network_icon_->GetIconAndText(NULL);
  SetIcon(bitmap);
  SchedulePaint();
}

////////////////////////////////////////////////////////////////////////////////
// NetworkDropdownButton, private methods:

void NetworkDropdownButton::SetNetworkIconAndText() {
  string16 text;
  const SkBitmap bitmap = network_icon_->GetIconAndText(&text);
  SetIcon(bitmap);
  SetText(UTF16ToWide(text));
}

}  // namespace chromeos
