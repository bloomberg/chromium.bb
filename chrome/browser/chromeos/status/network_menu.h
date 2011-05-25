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

namespace {

const int kMainCommandIndexOffset = 1000;
const int kVPNCommandIndexOffset  = 2000;
const int kMoreCommandIndexOffset = 3000;

}  // namespace

namespace gfx {
class Canvas;
}

namespace views {
class MenuItemView;
class MenuButton;
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
  virtual views::MenuButton* GetMenuButton() = 0;
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

  // Animation images. These are created lazily.
  static SkBitmap kAnimatingImages[];
  static SkBitmap kAnimatingImagesBlack[];

  // The network menu.
  scoped_ptr<views::MenuItemView> network_menu_;

  scoped_ptr<NetworkMenuModel> main_menu_model_;

  // Holds minimum width of the menu.
  int min_width_;

  // If true, call into the settings UI for network configuration dialogs.
  bool use_settings_ui_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenu);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_H_
