// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_H_

#include "base/basictypes.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "ui/gfx/native_widget_types.h"
#include "chrome/browser/chromeos/login/login_html_dialog.h"

namespace chromeos {

class NetworkMenuWebUI;

// Class which implements network dropdown menu using WebUI.
class NetworkDropdown : public NetworkMenu::Delegate,
                        public NetworkMenuIcon::Delegate,
                        NetworkLibrary::NetworkManagerObserver,
                        public LoginHtmlDialog::Delegate {
 public:
  NetworkDropdown(WebUI* web_ui, gfx::NativeWindow parent_window, bool oobe);
  virtual ~NetworkDropdown();

  // This method should be called, when item with the given id is chosen.
  void OnItemChosen(int id);

  // NetworkMenu::Delegate implementation:
  virtual views::MenuButton* GetMenuButton() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual void OpenButtonOptions() OVERRIDE;
  virtual bool ShouldOpenButtonOptions() const OVERRIDE;

  // NetworkMenuIcon::Delegate implementation:
  virtual void NetworkMenuIconChanged() OVERRIDE;

  // NetworkLibrary::NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(NetworkLibrary* cros) OVERRIDE;

  // Refreshes control state. Usually there's no need to do it manually
  // as control refreshes itself on network state change.
  // Should be called on language change.
  void Refresh();

 protected:
  // LoginHtmlDialog::Delegate implementation:
  virtual void OnDialogClosed() OVERRIDE;

 private:
  void SetNetworkIconAndText();

  gfx::NativeWindow parent_window_;

  // The Network menu.
  scoped_ptr<NetworkMenuWebUI> network_menu_;
  // The Network menu icon.
  scoped_ptr<NetworkMenuIcon> network_icon_;
  // Proxy settings dialog that can be invoked from network menu.
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

  WebUI* web_ui_;

  // Is the dropdown shown on one of the OOBE screens.
  bool oobe_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDropdown);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_H_
