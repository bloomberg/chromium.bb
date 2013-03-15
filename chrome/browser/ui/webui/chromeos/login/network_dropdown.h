// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_H_

#include "base/basictypes.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/net/connectivity_state_helper_observer.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebUI;
}

namespace chromeos {

class NetworkMenuWebUI;

// Class which implements network dropdown menu using WebUI.
class NetworkDropdown : public NetworkMenu::Delegate,
                        public NetworkMenuIcon::Delegate,
                        public ConnectivityStateHelperObserver {
 public:
  NetworkDropdown(content::WebUI* web_ui, bool oobe);
  virtual ~NetworkDropdown();

  // Sets last active network type. Used to show correct disconnected icon.
  void SetLastNetworkType(ConnectionType last_network_type);

  // This method should be called, when item with the given id is chosen.
  void OnItemChosen(int id);

  // NetworkMenu::Delegate implementation:
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual void OpenButtonOptions() OVERRIDE;
  virtual bool ShouldOpenButtonOptions() const OVERRIDE;

  // NetworkMenuIcon::Delegate implementation:
  virtual void NetworkMenuIconChanged() OVERRIDE;

  // ConnectivityStateHelperObserver implementation:
  virtual void NetworkManagerChanged() OVERRIDE;

  // Refreshes control state. Usually there's no need to do it manually
  // as control refreshes itself on network state change.
  // Should be called on language change.
  void Refresh();

 private:
  void SetNetworkIconAndText();

  // Forces network scan and refreshes control state. Should be called
  // by |network_scan_timer_| only.
  void ForceNetworkScan();

  // The Network menu.
  scoped_ptr<NetworkMenuWebUI> network_menu_;
  // The Network menu icon.
  scoped_ptr<NetworkMenuIcon> network_icon_;

  content::WebUI* web_ui_;

  // Is the dropdown shown on one of the OOBE screens.
  bool oobe_;

  // Timer used to periodically force network scan.
  base::RepeatingTimer<NetworkDropdown> network_scan_timer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDropdown);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_H_
