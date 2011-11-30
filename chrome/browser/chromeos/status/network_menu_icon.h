// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_ICON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_ICON_H_
#pragma once

#include <string>
#include <map>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"

namespace chromeos {

class NetworkIcon;

// Manages an icon reflecting the current state of the network.
// Also provides static functions for fetching network bitmaps.
class NetworkMenuIcon : public ui::AnimationDelegate {
 public:
  enum Mode {
    MENU_MODE,      // Prioritizes connecting networks and sets tooltips.
    DROPDOWN_MODE,  // Prioritizes connected networks and sets display text.
  };

  // Used for calls to GetBitmap() and GetNumBitmaps() below.
  enum BitmapType {
    ARCS = 0,
    BARS
  };

  class Delegate {
   public:
    // Called when the bitmap has changed due to animation. Callback should
    // trigger a call to GetIconAndText() to generate and retrieve the bitmap.
    virtual void NetworkMenuIconChanged() = 0;
  };

  NetworkMenuIcon(Delegate* delegate, Mode mode);
  virtual ~NetworkMenuIcon();

  // Setter for |last_network_type_|
  void set_last_network_type(ConnectionType last_network_type) {
    last_network_type_ = last_network_type;
  }

  // Generates and returns the icon bitmap. This will never return NULL.
  // Also sets |text| if not NULL. Behavior varies depending on |mode_|.
  const SkBitmap GetIconAndText(string16* text);

  // ui::AnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // Static functions for generating network icon bitmaps:

  // Composites the bitmaps to generate a network icon.
  static const SkBitmap GenerateBitmapFromComponents(
      const SkBitmap& icon,
      const SkBitmap* top_left_badge,
      const SkBitmap* top_right_badge,
      const SkBitmap* bottom_left_badge,
      const SkBitmap* bottom_right_badge);

  // Sets a blended bitmap for connecting images.
  static const SkBitmap GenerateConnectingBitmap(const SkBitmap& source);

  // Returns a bitmap associated with |network|, reflecting its current state.
  static const SkBitmap GetBitmap(const Network* network);

  // Returns a bitmap representing an unconnected VPN.
  static const SkBitmap GetVpnBitmap();

  // Access a specific bitmap. If index is out of range an empty bitmap
  // will be returned.
  static const SkBitmap GetBitmap(BitmapType type, int index);

  // Gets the disconnected bitmap for given type.
  static const SkBitmap GetDisconnectedBitmap(BitmapType type);

  // Gets the connected bitmap for given type.
  static const SkBitmap GetConnectedBitmap(BitmapType type);

  // Returns total number of bitmaps for given type.
  static int NumBitmaps(BitmapType type);

 protected:
  // Virtual for testing.
  virtual double GetAnimation();

 private:
  const Network* GetConnectingNetwork();
  void SetConnectingIcon(const Network* network, double animation);
  void SetIconAndText(string16* text);

  Mode mode_;
  Delegate* delegate_;
  SkBitmap empty_vpn_badge_;
  SkBitmap vpn_connecting_badge_;
  ui::ThrobAnimation animation_connecting_;
  ConnectionType last_network_type_;
  scoped_ptr<NetworkIcon> icon_;
  const Network* connecting_network_;  // weak pointer.

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuIcon);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_NETWORK_MENU_ICON_H_
