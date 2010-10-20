// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
#pragma once

#include <string>
#include <vector>

#include "app/menus/menu_model.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "gfx/native_widget_types.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Canvas;
}

namespace views {
class Menu2;
}

namespace chromeos {

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
// <icon>  Other...
// --------------------------------
//         Disable Wifi
//         Disable Celluar
// --------------------------------
//         <IP Address>
//         Network settings...
//
// <icon> will show the strength of the wifi/cellular networks.
// The label will be BOLD if the network is currently connected.
class NetworkMenu : public views::ViewMenuDelegate,
                    public menus::MenuModel {
 public:
  struct NetworkInfo {
    NetworkInfo() : need_passphrase(false), remembered(true) {}
    // "ethernet" | "wifi" | "cellular" | "other".
    std::string network_type;
    // "connected" | "connecting" | "disconnected" | "error".
    std::string status;
    // status message or error message, empty if unknown status.
    std::string message;
    // IP address (if network is active, empty otherwise)
    std::string ip_address;
    // true if the network requires a passphrase.
    bool need_passphrase;
    // true if the network is currently remembered.
    bool remembered;
  };

  NetworkMenu();
  virtual ~NetworkMenu();

  // Retrieves network info for the DOMUI NetworkMenu (NetworkMenuUI).
  // |index| is the index in menu_items_, set when the menu is built.
  bool GetNetworkAt(int index, NetworkInfo* info) const;

  // Connect or reconnect to the network at |index|.
  // If remember >= 0, set the favorite state of the network.
  // Returns true if a connect occurred (e.g. menu should be closed).
  bool ConnectToNetworkAt(int index,
                          const std::string& passphrase,
                          const std::string& ssid,
                          int remember) const;

  // menus::MenuModel implementation.
  virtual bool HasIcons() const  { return true; }
  virtual int GetItemCount() const;
  virtual menus::MenuModel::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const { return index; }
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const { return true; }
  virtual const gfx::Font* GetLabelFontAt(int index) const;
  virtual bool GetAcceleratorAt(int index,
      menus::Accelerator* accelerator) const { return false; }
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const { return 0; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const;
  virtual menus::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const {
    return NULL;
  }
  virtual bool IsEnabledAt(int index) const;
  virtual menus::MenuModel* GetSubmenuModelAt(int index) const { return NULL; }
  virtual void HighlightChangedTo(int index) {}
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow() {}

  void SetFirstLevelMenuWidth(int width);

  // Cancels the active menu.
  void CancelMenu();

  // Returns the Icon for a network strength between 0 and 100.
  // |black| is used to specify whether to return a black icon for display
  // on a light background or a white icon for display on a dark background.
  static SkBitmap IconForNetworkStrength(int strength, bool black);

  // Returns the Icon for a network strength for CellularNetwork |cellular|.
  // This returns different colored bars depending on cellular data left.
  static SkBitmap IconForNetworkStrength(CellularNetwork cellular);

  // This method will convert the |icon| bitmap to the correct size for display.
  // If the |badge| icon is not empty, it will draw that on top of the icon.
  static SkBitmap IconForDisplay(SkBitmap icon, SkBitmap badge);

 protected:
  virtual bool IsBrowserMode() const = 0;
  virtual gfx::NativeWindow GetNativeWindow() const = 0;
  virtual void OpenButtonOptions() const = 0;
  virtual bool ShouldOpenButtonOptions() const = 0;

  // Notify subclasses that connection to |network| was initiated.
  virtual void OnConnectNetwork(const Network& network,
                                SkBitmap selected_icon_) {}
  // Update the menu (e.g. when the network list or status has changed).
  void UpdateMenu();

 private:
  enum MenuItemFlags {
    FLAG_DISABLED          = 1 << 0,
    FLAG_TOGGLE_ETHERNET   = 1 << 1,
    FLAG_TOGGLE_WIFI       = 1 << 2,
    FLAG_TOGGLE_CELLULAR   = 1 << 3,
    FLAG_TOGGLE_OFFLINE    = 1 << 4,
    FLAG_ASSOCIATED        = 1 << 5,
    FLAG_ETHERNET          = 1 << 6,
    FLAG_WIFI              = 1 << 7,
    FLAG_CELLULAR          = 1 << 8,
    FLAG_OPTIONS           = 1 << 9,
    FLAG_OTHER_NETWORK     = 1 << 10,
  };

  struct MenuItem {
    MenuItem()
        : type(menus::MenuModel::TYPE_SEPARATOR),
          flags(0) {}
    MenuItem(menus::MenuModel::ItemType type, string16 label, SkBitmap icon,
             const std::string& wireless_path, int flags)
        : type(type),
          label(label),
          icon(icon),
          wireless_path(wireless_path),
          flags(flags) {}

    menus::MenuModel::ItemType type;
    string16 label;
    SkBitmap icon;
    std::string wireless_path;
    int flags;
  };
  typedef std::vector<MenuItem> MenuItemVector;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Called by RunMenu to initialize our list of menu items.
  void InitMenuItems();

  // Shows network details in DOM UI options window.
  void ShowTabbedNetworkSettings(const Network& network) const;

  // Show a NetworkConfigView modal dialog instance.
  // TODO(stevenjb): deprecate this once all of the UI is embedded in the menu.
  void ShowNetworkConfigView(NetworkConfigView* view, bool focus_login) const;

  // Wrappers for the ShowNetworkConfigView / ShowTabbedNetworkSettings.
  void ShowWifi(const WifiNetwork& wifi, bool focus_login) const;
  void ShowCellular(const CellularNetwork& cellular, bool focus_login) const;
  void ShowEthernet(const EthernetNetwork& ethernet) const;
  void ShowOther() const;

  // Set to true if we are currently refreshing the menu.
  bool refreshing_menu_;

  // The number of wifi strength images.
  static const int kNumWifiImages;

  // Bars image resources.
  static const int kBarsImages[];
  static const int kBarsImagesBlack[];
  static const int kBarsImagesLowData[];
  static const int kBarsImagesVLowData[];

  // Our menu items.
  MenuItemVector menu_items_;

  // The network menu.
  scoped_ptr<views::Menu2> network_menu_;

  // Holds minimum width or -1 if it wasn't set up.
  int min_width_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenu);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
