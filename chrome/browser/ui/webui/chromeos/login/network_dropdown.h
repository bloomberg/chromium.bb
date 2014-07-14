// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_H_

#include "base/basictypes.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "ui/chromeos/network/network_icon_animation_observer.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class WebUI;
}

namespace chromeos {

class NetworkMenuWebUI;
class NetworkState;

// Class which implements network dropdown menu using WebUI.
class NetworkDropdown : public NetworkMenu::Delegate,
                        public NetworkStateHandlerObserver,
                        public ui::network_icon::AnimationObserver {
 public:
  class Actor {
   public:
    virtual ~Actor() {}
    virtual void OnConnectToNetworkRequested() = 0;
  };

  NetworkDropdown(Actor* actor, content::WebUI* web_ui, bool oobe);
  virtual ~NetworkDropdown();

  // This method should be called, when item with the given id is chosen.
  void OnItemChosen(int id);

  // NetworkMenu::Delegate
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual void OpenButtonOptions() OVERRIDE;
  virtual bool ShouldOpenButtonOptions() const OVERRIDE;
  virtual void OnConnectToNetworkRequested() OVERRIDE;

  // NetworkStateHandlerObserver
  virtual void DefaultNetworkChanged(const NetworkState* network) OVERRIDE;
  virtual void NetworkConnectionStateChanged(
      const NetworkState* network) OVERRIDE;
  virtual void NetworkListChanged() OVERRIDE;

  // network_icon::AnimationObserver
  virtual void NetworkIconChanged() OVERRIDE;

  // Refreshes control state. Usually there's no need to do it manually
  // as control refreshes itself on network state change.
  // Should be called on language change.
  void Refresh();

 private:
  void SetNetworkIconAndText();

  // Request a network scan and refreshes control state. Should be called
  // by |network_scan_timer_| only.
  void RequestNetworkScan();

  // The Network menu.
  scoped_ptr<NetworkMenuWebUI> network_menu_;

  Actor* actor_;

  content::WebUI* web_ui_;

  // Is the dropdown shown on one of the OOBE screens.
  bool oobe_;

  // Timer used to periodically force network scan.
  base::RepeatingTimer<NetworkDropdown> network_scan_timer_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDropdown);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_DROPDOWN_H_
