// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
#pragma once

#include <string>
#include <vector>

#include "chrome/browser/chromeos/options/network_config_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/native_widget_types.h"
#include "views/controls/menu/view_menu_delegate.h"

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
                    public ui::MenuModel {
 public:
  struct NetworkInfo {
    NetworkInfo() :
        need_passphrase(false), remembered(true), auto_connect(true) {}
    // "ethernet" | "wifi" | "cellular" | "other".
    std::string network_type;
    // "connected" | "connecting" | "disconnected" | "error".
    std::string status;
    // status message or error message, empty if unknown status.
    std::string message;
    // IP address (if network is active, empty otherwise)
    std::string ip_address;
    // Remembered passphrase.
    std::string passphrase;
    // true if the network requires a passphrase.
    bool need_passphrase;
    // true if the network is currently remembered.
    bool remembered;
    // true if the network is auto connect (meaningful for Wifi only).
    bool auto_connect;
  };

  NetworkMenu();
  virtual ~NetworkMenu();

  // Retrieves network info for the WebUI NetworkMenu (NetworkMenuUI).
  // |index| is the index in menu_items_, set when the menu is built.
  bool GetNetworkAt(int index, NetworkInfo* info) const;

  // Connect or reconnect to the network at |index|.
  // If remember >= 0, set the favorite state of the network.
  // Returns true if a connect occurred (e.g. menu should be closed).
  bool ConnectToNetworkAt(int index,
                          const std::string& passphrase,
                          const std::string& ssid,
                          int remember) const;

  // ui::MenuModel implementation.
  virtual bool HasIcons() const  { return true; }
  virtual int GetItemCount() const;
  virtual ui::MenuModel::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const { return index; }
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsItemDynamicAt(int index) const { return true; }
  virtual const gfx::Font* GetLabelFontAt(int index) const;
  virtual bool GetAcceleratorAt(int index,
      ui::Accelerator* accelerator) const { return false; }
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const { return 0; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const;
  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const {
    return NULL;
  }
  virtual bool IsEnabledAt(int index) const;
  virtual ui::MenuModel* GetSubmenuModelAt(int index) const { return NULL; }
  virtual void HighlightChangedTo(int index) {}
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow() {}

  void SetFirstLevelMenuWidth(int width);

  // Cancels the active menu.
  void CancelMenu();

  // The following methods returns pointer to a shared instance of the SkBitmap.
  // This shared bitmap is owned by the resource bundle and should not be freed.

  // Returns the Icon for a network strength for a WifiNetwork |wifi|.
  // |black| is used to specify whether to return a black icon for display
  // on a light background or a white icon for display on a dark background.
  // Expected to never return NULL.
  static const SkBitmap* IconForNetworkStrength(const WifiNetwork* wifi,
                                                bool black);
  // Returns the Icon for a network strength for CellularNetwork |cellular|.
  // |black| is used to specify whether to return a black icon for display
  // on a light background or a white icon for display on a dark background.
  // Expected to never return NULL.
  static const SkBitmap* IconForNetworkStrength(const CellularNetwork* cellular,
                                                bool black);
  // Returns the Icon for animating network connecting.
  // |animation_value| is the value from Animation.GetCurrentValue()
  // |black| is used to specify whether to return a black icon for display
  // on a light background or a white icon for display on a dark background.
  // Expected to never return NULL.
  static const SkBitmap* IconForNetworkConnecting(double animation_value,
                                                  bool black);
  // Returns the Badge for a given network technology.
  // This returns different colored symbols depending on cellular data left.
  // Returns NULL if not badge is needed.
  static const SkBitmap* BadgeForNetworkTechnology(
      const CellularNetwork* cellular);
  // Returns the Badge for a given network roaming status.
  // This returns "R" badge if network is in roaming state, otherwise
  // returns NULL. Badge is supposed to be shown on top right of the icon.
  static const SkBitmap* BadgeForRoamingStatus(const CellularNetwork* cellular);
  // This method will convert the |icon| bitmap to the correct size for display.
  // |icon| must be non-NULL.
  // If the |badge| icon is not NULL, it will draw that on top of the icon.
  static SkBitmap IconForDisplay(const SkBitmap* icon, const SkBitmap* badge);

  // This method will convert the |icon| bitmap to the correct size for display.
  // |icon| must be non-NULL.
  // If one of the |bottom_right_badge| or |top_left_badge| icons are not NULL,
  // they will be drawn on top of the icon.
  static SkBitmap IconForDisplay(const SkBitmap* icon,
                                 const SkBitmap* bottom_right_badge,
                                 const SkBitmap* top_left_badge);

 protected:
  virtual bool IsBrowserMode() const = 0;
  virtual gfx::NativeWindow GetNativeWindow() const = 0;
  virtual void OpenButtonOptions() = 0;
  virtual bool ShouldOpenButtonOptions() const = 0;

  // Notify subclasses that connection to |network| was initiated.
  virtual void OnConnectNetwork(const Network* network,
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
        : type(ui::MenuModel::TYPE_SEPARATOR),
          flags(0) {}
    MenuItem(ui::MenuModel::ItemType type, string16 label, SkBitmap icon,
             const std::string& wireless_path, int flags)
        : type(type),
          label(label),
          icon(icon),
          wireless_path(wireless_path),
          flags(flags) {}

    ui::MenuModel::ItemType type;
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

  // Shows network details in Web UI options window.
  void ShowTabbedNetworkSettings(const Network* network) const;

  // Show a NetworkConfigView modal dialog instance.
  // TODO(stevenjb): deprecate this once all of the UI is embedded in the menu.
  void ShowNetworkConfigView(NetworkConfigView* view) const;

  void ActivateCellular(const CellularNetwork* cellular) const;
  void ShowOther() const;

  // Set to true if we are currently refreshing the menu.
  bool refreshing_menu_;

  // The number of bars images for representing network strength.
  static const int kNumBarsImages;

  // Bars image resources.
  static const int kBarsImages[];
  static const int kBarsImagesBlack[];
  // TODO(chocobo): Add these back when we decide to do colored bars again.
//  static const int kBarsImagesLowData[];
//  static const int kBarsImagesVLowData[];

  // The number of animating images for network connecting.
  static const int kNumAnimatingImages;
  // Animation images. These are created lazily.
  static SkBitmap kAnimatingImages[];
  static SkBitmap kAnimatingImagesBlack[];

  // Our menu items.
  MenuItemVector menu_items_;

  // The network menu.
  scoped_ptr<views::Menu2> network_menu_;

  // Holds minimum width or -1 if it wasn't set up.
  int min_width_;

  // If true, call into the settings UI for network configuration dialogs.
  bool use_settings_ui_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenu);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
