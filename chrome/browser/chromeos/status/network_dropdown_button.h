// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_DROPDOWN_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_DROPDOWN_BUTTON_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/chromeos/views/dropdown_button.h"

namespace chromeos {

// The network dropdown button with menu. Used on welcome screen.
// This class will handle getting the networks to show connected network
// at top level and populating the menu.
// See NetworkMenu for more details.
class NetworkDropdownButton : public DropDownButton,
                              public views::ViewMenuDelegate,
                              public NetworkMenu::Delegate,
                              public NetworkMenuIcon::Delegate,
                              public NetworkLibrary::NetworkManagerObserver,
                              public LoginHtmlDialog::Delegate {
 public:
  NetworkDropdownButton(bool is_browser_mode,
                        gfx::NativeWindow parent_window,
                        bool should_show_options);
  virtual ~NetworkDropdownButton();

  void SetFirstLevelMenuWidth(int width);

  void CancelMenu();

  // Refreshes button state. Used when language has been changed.
  void Refresh();

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj) OVERRIDE;

  // NetworkMenu::Delegate implementation:
  virtual views::MenuButton* GetMenuButton() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual void OpenButtonOptions() OVERRIDE;
  virtual bool ShouldOpenButtonOptions() const OVERRIDE;

  // NetworkMenuIcon::Delegate implementation:
  virtual void NetworkMenuIconChanged() OVERRIDE;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

 protected:
  // Overridden from views::View.
  virtual void OnLocaleChanged() OVERRIDE;

  // LoginHtmlDialog::Delegate implementation:
  virtual void OnDialogClosed() OVERRIDE;

 private:
  void SetNetworkIconAndText();

  // The Network menu.
  scoped_ptr<NetworkMenu> network_menu_;

  // The Network menu icon.
  scoped_ptr<NetworkMenuIcon> network_icon_;

  gfx::NativeWindow parent_window_;

  // If true things like proxy settings menu item will be supported.
  bool should_show_options_;

  // Proxy settings dialog that can be invoked from network menu.
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDropdownButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_DROPDOWN_BUTTON_H_
