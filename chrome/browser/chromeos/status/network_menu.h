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
#include "ui/base/models/menu_model.h"
#include "ui/gfx/native_widget_types.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace gfx {
class Canvas;
}

namespace views {
class Menu2;
}

namespace chromeos {

class NetworkMenu;

class NetworkMenuModel : public ui::MenuModel {
 public:
  struct NetworkInfo {
    NetworkInfo();
    ~NetworkInfo();

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

  explicit NetworkMenuModel(NetworkMenu* owner);
  virtual ~NetworkMenuModel();

  // Connect or reconnect to the network at |index|.
  // If remember >= 0, set the favorite state of the network.
  // Returns true if a connect occurred (e.g. menu should be closed).
  bool ConnectToNetworkAt(int index,
                          const std::string& passphrase,
                          const std::string& ssid,
                          int remember) const;

  // Called by NetworkMenu::RunMenu to initialize list of menu items.
  virtual void InitMenuItems(bool is_browser_mode,
                             bool should_open_button_options) = 0;

  // ui::MenuModel implementation.
  virtual bool HasIcons() const;
  virtual int GetItemCount() const;
  virtual ui::MenuModel::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsItemDynamicAt(int index) const;
  virtual const gfx::Font* GetLabelFontAt(int index) const;
  virtual bool GetAcceleratorAt(int index, ui::Accelerator* accelerator) const;
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const;
  virtual bool GetIconAt(int index, SkBitmap* icon);
  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const;
  virtual bool IsEnabledAt(int index) const;
  virtual ui::MenuModel* GetSubmenuModelAt(int index) const;
  virtual void HighlightChangedTo(int index);
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow();
  virtual void SetMenuModelDelegate(ui::MenuModelDelegate* delegate);

 protected:
  enum MenuItemFlags {
    FLAG_NONE              = 0,
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
    FLAG_ADD_WIFI          = 1 << 10,
    FLAG_ADD_CELLULAR      = 1 << 11,
    FLAG_VPN               = 1 << 12,
    FLAG_ADD_VPN           = 1 << 13,
    FLAG_DISCONNECT_VPN    = 1 << 14,
    FLAG_VIEW_ACCOUNT      = 1 << 15,
  };

  struct MenuItem {
    MenuItem()
        : type(ui::MenuModel::TYPE_SEPARATOR),
          sub_menu_model(NULL),
          flags(0) {}
    MenuItem(ui::MenuModel::ItemType type, string16 label, SkBitmap icon,
             const std::string& service_path, int flags)
        : type(type),
          label(label),
          icon(icon),
          service_path(service_path),
          sub_menu_model(NULL),
          flags(flags) {}
    MenuItem(ui::MenuModel::ItemType type, string16 label, SkBitmap icon,
             NetworkMenuModel* sub_menu_model, int flags)
        : type(type),
          label(label),
          icon(icon),
          sub_menu_model(sub_menu_model),
          flags(flags) {}

    ui::MenuModel::ItemType type;
    string16 label;
    SkBitmap icon;
    std::string service_path;
    NetworkMenuModel* sub_menu_model;  // Weak.
    int flags;
  };
  typedef std::vector<MenuItem> MenuItemVector;

  // Our menu items.
  MenuItemVector menu_items_;

  NetworkMenu* owner_;  // Weak pointer to NetworkMenu that owns this MenuModel.

  // Top up URL of the current carrier on empty string if there's none.
  std::string top_up_url_;

  // Carrier ID which top up URL is initialized for.
  // Used to update top up URL only when cellular carrier has changed.
  std::string carrier_id_;

 private:
  // Show a NetworkConfigView modal dialog instance.
  void ShowNetworkConfigView(NetworkConfigView* view) const;

  void ActivateCellular(const CellularNetwork* cellular) const;
  void ShowOther(ConnectionType type) const;
  void ShowOtherCellular() const;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuModel);
};

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
class NetworkMenu : public views::ViewMenuDelegate {
 public:
  NetworkMenu();
  virtual ~NetworkMenu();

  void SetFirstLevelMenuWidth(int width);

  // Cancels the active menu.
  void CancelMenu();

  virtual bool IsBrowserMode() const = 0;

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
  // Returns the badge for the given network if it's active with vpn.
  // If |network| is not null, will check if it's the active network.
  // If |network| is null or if |network| is the active one, the yellow lock
  // badge will be returned, otherwise returns null.
  // Badge is supposed to be shown on in bottom left corner of the icon.
  static const SkBitmap* BadgeForPrivateNetworkStatus(const Network* network);

  // This method will convert the |icon| bitmap to the correct size for display.
  // |icon| must be non-NULL.
  // If |badge| icon is not NULL, it will be drawn on top of the icon in
  // the bottom-right corner.
  static SkBitmap IconForDisplay(const SkBitmap* icon, const SkBitmap* badge);
  // This method will convert the |icon| bitmap to the correct size for display.
  // |icon| must be non-NULL.
  // If one of the |bottom_right_badge| or |top_left_badge| or
  // |bottom_left_badge| icons are not NULL, they will be drawn on top of the
  // icon.
  static SkBitmap IconForDisplay(const SkBitmap* icon,
                                 const SkBitmap* bottom_right_badge,
                                 const SkBitmap* top_left_badge,
                                 const SkBitmap* bottom_left_badge);

 protected:
  virtual gfx::NativeWindow GetNativeWindow() const = 0;
  virtual void OpenButtonOptions() = 0;
  virtual bool ShouldOpenButtonOptions() const = 0;

  // Notify subclasses that connection to |network| was initiated.
  virtual void OnConnectNetwork(const Network* network,
                                SkBitmap selected_icon_) {}

  // Shows network details in Web UI options window.
  void ShowTabbedNetworkSettings(const Network* network) const;

  // Update the menu (e.g. when the network list or status has changed).
  void UpdateMenu();

 private:
  friend class NetworkMenuModel;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Set to true if we are currently refreshing the menu.
  bool refreshing_menu_;

  // The number of bars images for representing network strength.
  static const int kNumBarsImages;

  // Bars image resources.
  static const int kBarsImages[];
  static const int kBarsImagesBlack[];
  static const int kBarsImagesOrange[];
  // TODO(chocobo): Add this back when we decide to do colored bars again.
  // static const int kBarsImagesVLowData[];

  // The number of animating images for network connecting.
  static const int kNumAnimatingImages;
  // Animation images. These are created lazily.
  static SkBitmap kAnimatingImages[];
  static SkBitmap kAnimatingImagesBlack[];

  // The network menu.
  scoped_ptr<views::Menu2> network_menu_;

  scoped_ptr<NetworkMenuModel> main_menu_model_;

  // Holds minimum width or -1 if it wasn't set up.
  int min_width_;

  // If true, call into the settings UI for network configuration dialogs.
  bool use_settings_ui_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenu);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
