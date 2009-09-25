// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_MENU_BUTTON_H_

#include <vector>

#include "chrome/browser/chromeos/password_dialog_view.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

class Browser;

// The network menu button in the status area.
// This class will handle getting the wifi networks and populating the menu.
// It will also handle the status icon changing and connecting to another
// wifi network.
class NetworkMenuButton : public views::MenuButton,
                          public views::ViewMenuDelegate,
                          public views::Menu2Model,
                          public PasswordDialogDelegate {
 public:
  struct WifiNetwork {
    WifiNetwork() { }
    WifiNetwork(const string16& ssid, const string16& encryption, int strength)
        : ssid(ssid),
          encryption(encryption),
          strength(strength) { }

    string16 ssid;
    string16 encryption;
    int strength;
  };

  explicit NetworkMenuButton(Browser* browser);
  virtual ~NetworkMenuButton() {}

  // views::Menu2Model implementation.
  virtual bool HasIcons() const  { return false; }
  virtual int GetItemCount() const;
  virtual views::Menu2Model::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const;
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const { return true; }
  virtual bool GetAcceleratorAt(int index,
      views::Accelerator* accelerator) const { return false; }
  virtual bool IsItemCheckedAt(int index) const;
  virtual int GetGroupIdAt(int index) const { return 0; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const { return false; }
  virtual bool IsEnabledAt(int index) const;
  virtual Menu2Model* GetSubmenuModelAt(int index) const { return NULL; }
  virtual void HighlightChangedTo(int index) {}
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow() {}

  // PasswordDialogDelegate implementation.
  virtual bool OnPasswordDialogCancel();
  virtual bool OnPasswordDialogAccept(const string16& password);

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt,
                       gfx::NativeView hwnd);

  // Helper method to add a wifi network to the model.
  void AddWifiNetwork(const string16& ssid, const string16& encryption,
      int strength);

  // Refreshes the wifi networks model using real data.
  void RefreshWifiNetworks();

  // Connect to the specified wireless network with password.
  // Returns whether or not connection was successful.
  bool ConnectToWifiNetwork(const string16& ssid, const string16& password);

  // Gets the wifi image for specified wifi network.
  SkBitmap GetWifiImage(WifiNetwork wifi_network) const;

  // Set to true if we are currently refreshing the menu.
  bool refreshing_menu_;

  // The number of wifi strength images.
  static const int kNumWifiImages;

  // Wifi strength images.
  static SkBitmap* wifi_images_[];

  // Wired image.
  static SkBitmap* wired_image_;

  // Disconnected image.
  static SkBitmap* disconnected_image_;

  // The currently connected wifi network ssid.
  string16 current_ssid_;

  // The wifi netowrk ssid we are attempting to connect to.
  string16 connecting_ssid_;

  // A list of wifi networks.
  std::vector<WifiNetwork> wifi_networks_;

  // The network menu.
  views::Menu2 network_menu_;

  // The browser window that owns us.
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuButton);
};

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_MENU_BUTTON_H_
