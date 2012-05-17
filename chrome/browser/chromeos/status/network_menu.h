// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
#pragma once

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/cros/network_library.h"  // ConnectionType
#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow
#include "ui/views/controls/button/menu_button_listener.h"

class Browser;

namespace ui {
class MenuModel;
}

namespace views {
class MenuItemView;
class MenuButton;
class MenuModelAdapter;
class MenuRunner;
}

namespace chromeos {

class NetworkMenuModel;

// Menu for network menu button in the status area/welcome screen.
// This class will populating the menu with the list of networks.
// It will also handle connecting to another wifi/cellular network.
//
// The network menu looks like this:
//
// <icon>  Ethernet
// <icon>  Wifi Network A
// <icon>  Wifi Network B
// <icon>  Wifi Network C
// <icon>  Cellular Network A
// <icon>  Cellular Network B
// <icon>  Cellular Network C
// <icon>  Other Wi-Fi network...
// --------------------------------
// <icon>  Private networks ->
//         <icon>  Virtual Network A
//         <icon>  Virtual Network B
//         ----------------------------------
//                 Add private network...
//                 Disconnect private network
// --------------------------------
//         Disable Wifi
//         Disable Celluar
// --------------------------------
//         <IP Address>
//         Network settings...
//
// <icon> will show the strength of the wifi/cellular networks.
// The label will be BOLD if the network is currently connected.

class NetworkMenu {
 public:
  class Delegate {
   public:
    virtual views::MenuButton* GetMenuButton() = 0;
    virtual gfx::NativeWindow GetNativeWindow() const = 0;
    virtual void OpenButtonOptions() = 0;
    virtual bool ShouldOpenButtonOptions() const = 0;
  };

  explicit NetworkMenu(Delegate* delegate);
  virtual ~NetworkMenu();

  // Access to menu definition.
  ui::MenuModel* GetMenuModel();

  // Cancels the active menu.
  void CancelMenu();

  // Update the menu (e.g. when the network list or status has changed).
  virtual void UpdateMenu();

  // Run the menu.
  void RunMenu(views::View* source);

  // Shows network details in Web UI options window.
  void ShowTabbedNetworkSettings(const Network* network) const;

  // Getters.
  Delegate* delegate() const { return delegate_; }

  // Setters.
  void set_min_width(int min_width) { min_width_ = min_width; }

  // Attempts to connect to the specified network. If the network is already
  // connected, or is connecting, then it shows the settings for the network.
  void ConnectToNetwork(Network* network);

  // Used in a closure for doing actual network connection.
  void DoConnect(Network* network);

  // Enables/disables wifi/cellular network device.
  void ToggleWifi();
  void ToggleMobile();

  // Shows UI to user to connect to an unlisted wifi network.
  void ShowOtherWifi();

  // Shows UI to user to search for cellular networks.
  void ShowOtherCellular();

  // Decides whether a network should be highlighted in the UI.
  bool ShouldHighlightNetwork(const Network* network);

 private:
  friend class NetworkMenuModel;

  // Returns the last active browser. If there is no such browser, creates a new
  // browser window with an empty tab and returns it.
  Browser* GetAppropriateBrowser() const;

  // Weak ptr to delegate.
  Delegate* delegate_;

  // Set to true if we are currently refreshing the menu.
  bool refreshing_menu_;

  // The network menu.
  scoped_ptr<NetworkMenuModel> main_menu_model_;
  scoped_ptr<views::MenuModelAdapter> menu_model_adapter_;
  views::MenuItemView* menu_item_view_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  // Holds minimum width of the menu.
  int min_width_;

  // Weak pointer factory so we can start connections at a later time
  // without worrying that they will actually try to happen after the lifetime
  // of this object.
  base::WeakPtrFactory<NetworkMenu> weak_pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenu);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
