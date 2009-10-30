// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_MENU_BUTTON_H_

#include <string>

#include "app/throb_animation.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/network_library.h"
#include "chrome/browser/chromeos/password_dialog_view.h"
#include "chrome/browser/chromeos/status_area_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

class Browser;
class SkBitmap;

namespace gfx {
class Canvas;
}

namespace chromeos {

// The network menu button in the status area.
// This class will handle getting the wifi networks and populating the menu.
// It will also handle the status icon changing and connecting to another
// wifi network.
class NetworkMenuButton : public StatusAreaButton,
                          public views::ViewMenuDelegate,
                          public views::Menu2Model,
                          public PasswordDialogDelegate,
                          public NetworkLibrary::Observer {
 public:
  explicit NetworkMenuButton(gfx::NativeWindow browser_window);
  virtual ~NetworkMenuButton();

  // views::Menu2Model implementation.
  virtual bool HasIcons() const  { return false; }
  virtual int GetItemCount() const;
  virtual views::Menu2Model::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const { return index; }
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
  virtual bool OnPasswordDialogCancel() { return true; }
  virtual bool OnPasswordDialogAccept(const std::string& ssid,
                                      const string16& password);

  // AnimationDelegate implementation.
  virtual void AnimationProgressed(const Animation* animation);

  // NetworkLibrary::Observer implementation.
  virtual void NetworkChanged(NetworkLibrary* obj);
  virtual void NetworkTraffic(NetworkLibrary* cros, int traffic_type);

 protected:
  // StatusAreaButton implementation.
  virtual void DrawIcon(gfx::Canvas* canvas);

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Set to true if we are currently refreshing the menu.
  bool refreshing_menu_;

  // The number of wifi strength images.
  static const int kNumWifiImages;

  // The minimum opacity of the wifi bars.
  static const int kMinOpacity;

  // The maximum opacity of the wifi bars.
  static const int kMaxOpacity;

  // A list of wifi networks.
  WifiNetworkVector wifi_networks_;

  // The activated wifi network.
  WifiNetwork activated_wifi_network_;

  // The network menu.
  views::Menu2 network_menu_;

  // Our parent window
  gfx::NativeWindow browser_window_;

  // The throb animation that does the wifi connecting animation.
  ThrobAnimation animation_connecting_;

  // The throb animation that does the downloading animation.
  ThrobAnimation animation_downloading_;

  // The throb animation that does the uploading animation.
  ThrobAnimation animation_uploading_;

  // The duration of the icon throbbing in milliseconds.
  static const int kThrobDuration;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_MENU_BUTTON_H_
