// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_ICON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_ICON_H_

// NetworkMenuIcon Manages an icon that reflects the current state of the
// network (see chromeos::NetworkLibrary). It takes an optional Delegate
// argument in the constructor that signals the delegate when the icon changes.
// Example usage:
//   class MyIconDelegate : public NetworkMenuIcon::Delegate {
//     virtual void NetworkMenuIconChanged() OVERRIDE {
//       string16 tooltip;
//       const ImageSkia* image = network_icon_->GetIconAndText(&tooltip);
//       SetIcon(*bitmap);
//       SetTooltip(tooltip);
//       SchedulePaint();
//     }
//   }
//   MyIconDelegate my_delegate;
//   NetworkMenuIcon icon(&my_delegate, NetworkMenuIcon::MENU_MODE);
//
// NetworkMenuIcon also provides static functions for fetching network images
// (e.g. for network entries in the menu or settings).
// Example usage:
//   Network* network = network_library->FindNetworkByPath(my_network_path_);
//   SetIcon(NetworkMenuIcon::GetBitmap(network);
//
// This class is not explicitly thread-safe and functions are expected to be
// called from the UI thread.

#include <map>
#include <string>

#include "ash/system/chromeos/network/network_icon_animation_observer.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

class NetworkIcon;

class NetworkMenuIcon : public ash::network_icon::AnimationObserver {
 public:
  enum Mode {
    MENU_MODE,      // Prioritizes connecting networks and sets tooltips.
    DROPDOWN_MODE,  // Prioritizes connected networks and sets display text.
  };

  enum ResourceColorTheme {
    COLOR_DARK,
    COLOR_LIGHT,
  };

  // Used for calls to GetBitmap() and GetNumBitmaps() below.
  enum ImageType {
    ARCS = 0,
    BARS
  };

  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    // Called when the image has changed due to animation. The callback should
    // trigger a call to GetIconAndText() to generate and retrieve the image.
    virtual void NetworkMenuIconChanged() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // NetworkMenuIcon is owned by the caller. |delegate| can be NULL.
  // |mode| determines the menu behavior (see enum).
  NetworkMenuIcon(Delegate* delegate, Mode mode);
  virtual ~NetworkMenuIcon();

  // Sets the resource color theme (e.g. light or dark icons).
  void SetResourceColorTheme(ResourceColorTheme color);

  // Returns true if the icon should be visible in a system tray.
  bool ShouldShowIconInTray();

  // Generates and returns the icon image. If |text| is not NULL, sets it to
  // the tooltip or display text to show, based on the value of mode_.
  const gfx::ImageSkia GetIconAndText(string16* text);
  // Generates and returns the icon image for vpn network connection.
  const gfx::ImageSkia GetVpnIconAndText(string16* text);

  // ash::network_icon::AnimationObserver implementation.
  virtual void NetworkIconChanged() OVERRIDE;

  // Static functions for generating network icon images:

  // Composites the images to generate a network icon. Input parameters are
  // the icon and badges that are composited to generate |result|. Public
  // primarily for unit tests.
  static const gfx::ImageSkia GenerateImageFromComponents(
      const gfx::ImageSkia& icon,
      const gfx::ImageSkia* top_left_badge,
      const gfx::ImageSkia* top_right_badge,
      const gfx::ImageSkia* bottom_left_badge,
      const gfx::ImageSkia* bottom_right_badge);

  // Returns a modified version of |source| representing the connecting state
  // of a network. Public for unit tests.
  static const gfx::ImageSkia GenerateConnectingImage(
      const gfx::ImageSkia& source);

  // Returns an image associated with |network|, reflecting its current state.
  static const gfx::ImageSkia GetImage(const Network* network,
                                       ResourceColorTheme color);

  // Access a specific image of the specified color theme. If index is out of
  // range, an empty image will be returned.
  static const gfx::ImageSkia GetImage(ImageType type,
                                       int index,
                                       ResourceColorTheme color);

  // Gets the disconnected image for given type.
  static const gfx::ImageSkia GetDisconnectedImage(ImageType type,
                                                   ResourceColorTheme color);

  // Gets the connected image for given type.
  static const gfx::ImageSkia GetConnectedImage(ImageType type,
                                                ResourceColorTheme color);

  // Gets a network image for VPN.
  static gfx::ImageSkia* GetVirtualNetworkImage();

  // Returns total number of images for given type.
  static int NumImages(ImageType type);

 protected:
  // Starts the connection animation if necessary and returns its current value.
  // Virtual so that unit tests can override this.
  virtual double GetAnimation();

 private:
  // Returns the index for a connecting icon.
  int GetConnectingIndex();
  // Returns the appropriate connecting network if any.
  const Network* GetConnectingNetwork();
  // Sets the icon based on the state of the network and the network library.
  // Sets text_ to the appropriate tooltip or display text.
  // Returns true if the icon should animate.
  bool SetIconAndText();
  // Sets the icon and text for VPN connection.
  // Returns true if the icon should animate.
  bool SetVpnIconAndText();
  // Set the icon and text to show a warning if unable to load the cros library.
  void SetWarningIconAndText();
  // Sets the icon and text when displaying a connecting state.
  void SetConnectingIconAndText(const Network* connecting_network);
  // Sets the icon and text when connected to |network|.
  // Returns true if the icon should animate.
  bool SetActiveNetworkIconAndText(const Network* network);
  // Sets the icon and text when disconnected.
  void SetDisconnectedIconAndText();

  // Specifies whether this icon is for a normal menu or a dropdown menu.
  Mode mode_;
  // A delegate may be specified to receive notifications when this animates.
  Delegate* delegate_;
  // Generated image for connecting to a VPN.
  gfx::ImageSkia vpn_connecting_badge_;
  ResourceColorTheme resource_color_theme_;
  // The generated icon image.
  scoped_ptr<NetworkIcon> icon_;
  // A weak pointer to the currently connecting network. Used only for
  // comparison purposes; accessing this directly may be invalid.
  const Network* connecting_network_;
  // Index of current connecting animation.
  int connecting_index_;
  // The tooltip or display text associated with the menu icon.
  string16 text_;
  // Timer to eliminate noise while initializing cellular.
  base::Time initialize_state_time_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuIcon);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_ICON_H_
