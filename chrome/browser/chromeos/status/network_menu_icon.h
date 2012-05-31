// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_ICON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_ICON_H_
#pragma once

// NetworkMenuIcon Manages an icon that reflects the current state of the
// network (see chromeos::NetworkLibrary). It takes an optional Delegate
// argument in the constructor that signals the delegate when the icon changes.
// Example usage:
//   class MyIconDelegate : public NetworkMenuIcon::Delegate {
//     virtual void NetworkMenuIconChanged() OVERRIDE {
//       string16 tooltip;
//       const SkBitmap* bitmap = network_icon_->GetIconAndText(&tooltip);
//       SetIcon(*bitmap);
//       SetTooltip(tooltip);
//       SchedulePaint();
//     }
//   }
//   MyIconDelegate my_delegate;
//   NetworkMenuIcon icon(&my_delegate, NetworkMenuIcon::MENU_MODE);
//
// NetworkMenuIcon also provides static functions for fetching network bitmaps
// (e.g. for network entries in the menu or settings).
// Example usage:
//   Network* network = network_library->FindNetworkByPath(my_network_path_);
//   SetIcon(NetworkMenuIcon::GetBitmap(network);
//
// This class is not explicitly thread-safe and functions are expected to be
// called from the UI thread.

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"

namespace chromeos {

class NetworkIcon;

class NetworkMenuIcon : public ui::AnimationDelegate {
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
  enum BitmapType {
    ARCS = 0,
    BARS
  };

  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    // Called when the bitmap has changed due to animation. The callback should
    // trigger a call to GetIconAndText() to generate and retrieve the bitmap.
    virtual void NetworkMenuIconChanged() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // NetworkMenuIcon is owned by the caller. |delegate| can be NULL.
  // |mode| determines the menu behavior (see enum).
  NetworkMenuIcon(Delegate* delegate, Mode mode);
  virtual ~NetworkMenuIcon();

  // Setter for |last_network_type_|
  void set_last_network_type(ConnectionType last_network_type) {
    last_network_type_ = last_network_type;
  }

  // Sets the resource color theme (e.g. light or dark icons).
  void SetResourceColorTheme(ResourceColorTheme color);

  // Generates and returns the icon bitmap. If |text| is not NULL, sets it to
  // the tooltip or display text to show, based on the value of mode_.
  const SkBitmap GetIconAndText(string16* text);

  // ui::AnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Static functions for generating network icon bitmaps:

  // Composites the bitmaps to generate a network icon. Input parameters are
  // the icon and badges that are composited to generate |result|. Public
  // primarily for unit tests.
  static const SkBitmap GenerateBitmapFromComponents(
      const SkBitmap& icon,
      const SkBitmap* top_left_badge,
      const SkBitmap* top_right_badge,
      const SkBitmap* bottom_left_badge,
      const SkBitmap* bottom_right_badge);

  // Returns a modified version of |source| representing the connecting state
  // of a network. Public for unit tests.
  static const SkBitmap GenerateConnectingBitmap(const SkBitmap& source);

  // Returns a bitmap associated with |network|, reflecting its current state.
  static const SkBitmap GetBitmap(const Network* network,
                                  ResourceColorTheme color);

  // Returns a bitmap representing an unconnected VPN.
  static const SkBitmap GetVpnBitmap();

  // Access a specific bitmap of the specified color theme. If index is out of
  // range, an empty bitmap will be returned.
  static const SkBitmap GetBitmap(BitmapType type,
                                  int index,
                                  ResourceColorTheme color);

  // Gets the disconnected bitmap for given type.
  static const SkBitmap GetDisconnectedBitmap(BitmapType type,
                                              ResourceColorTheme color);

  // Gets the connected bitmap for given type.
  static const SkBitmap GetConnectedBitmap(BitmapType type,
                                           ResourceColorTheme color);

  // Returns total number of bitmaps for given type.
  static int NumBitmaps(BitmapType type);

 protected:
  // Starts the connection animation if necessary and returns its current value.
  // Virtual so that unit tests can override this.
  virtual double GetAnimation();

 private:
  // Returns the appropriate connecting network if any.
  const Network* GetConnectingNetwork();
  // Sets the icon based on the state of the network and the network library.
  // Sets text_ to the appropriate tooltip or display text.
  void SetIconAndText();
  // Set the icon and text to show a warning if unable to load the cros library.
  void SetWarningIconAndText();
  // Sets the icon and text when displaying a connecting state.
  void SetConnectingIconAndText();
  // Sets the icon and text when connected to |network|.
  void SetActiveNetworkIconAndText(const Network* network);
  // Sets the icon and text when disconnected.
  void SetDisconnectedIconAndText();

  // Specifies whether this icon is for a normal menu or a dropdown menu.
  Mode mode_;
  // A delegate may be specified to receive notifications when this animates.
  Delegate* delegate_;
  // Generated bitmap for connecting to a VPN.
  SkBitmap vpn_connecting_badge_;
  ResourceColorTheme resource_color_theme_;
  // Animation throbber for animating the icon while conencting.
  ui::ThrobAnimation animation_connecting_;
  // Cached type of previous displayed network.
  ConnectionType last_network_type_;
  // The generated icon image.
  scoped_ptr<NetworkIcon> icon_;
  // A weak pointer to the currently connecting network. Used only for
  // comparison purposes; accessing this directly may be invalid.
  const Network* connecting_network_;
  // The tooltip or display text associated with the menu icon.
  string16 text_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuIcon);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_ICON_H_
