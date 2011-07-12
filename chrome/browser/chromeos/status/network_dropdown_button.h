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
#include "chrome/browser/chromeos/views/dropdown_button.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"

namespace chromeos {

// The network dropdown button with menu. Used on welcome screen.
// This class will handle getting the networks to show connected network
// at top level and populating the menu.
// See NetworkMenu for more details.
class NetworkDropdownButton : public DropDownButton,
                              public views::ViewMenuDelegate,
                              public NetworkMenu::Delegate,
                              public NetworkLibrary::NetworkManagerObserver,
                              public LoginHtmlDialog::Delegate {
 public:
  NetworkDropdownButton(bool is_browser_mode,
                        gfx::NativeWindow parent_window,
                        bool should_show_options);
  virtual ~NetworkDropdownButton();

  // ui::AnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation);

  // NetworkLibrary::NetworkManagerObserver implementation.
  virtual void OnNetworkManagerChanged(NetworkLibrary* obj);

  void SetFirstLevelMenuWidth(int width);

  void CancelMenu();

  // Refreshes button state. Used when language has been changed.
  void Refresh();

  // NetworkMenu::Delegate implementation:
  virtual views::MenuButton* GetMenuButton();
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual void OpenButtonOptions();
  virtual bool ShouldOpenButtonOptions() const;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

 protected:
  // Overridden from views::View.
  virtual void OnLocaleChanged() OVERRIDE;

  // LoginHtmlDialog::Delegate implementation:
  virtual void OnDialogClosed() OVERRIDE;

 private:
  // The Network menu.
  scoped_ptr<NetworkMenu> network_menu_;

  // The throb animation that does the wifi connecting animation.
  ui::ThrobAnimation animation_connecting_;

  gfx::NativeWindow parent_window_;

  // If true things like proxy settings menu item will be supported.
  bool should_show_options_;

  // The last network we connected to (or tried to).
  ConnectionType last_network_type_;

  // Proxy settings dialog that can be invoked from network menu.
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDropdownButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_DROPDOWN_BUTTON_H_
