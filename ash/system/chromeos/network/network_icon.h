// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_H_
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_H_

#include <string>

#include "base/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
class NetworkState;
}

namespace ash {
namespace network_icon {

class AnimationObserver;

// Type of icon which dictates color theme and VPN badging
enum IconType {
  ICON_TYPE_TRAY,  // light icons with VPN badges
  ICON_TYPE_DEFAULT_VIEW,  // dark icons with VPN badges
  ICON_TYPE_LIST,  // dark icons without VPN badges
};

// Gets the image for the network associated with |service_path|. |network| must
// not be NULL. |icon_type| determines the color theme and whether or not to
// show the VPN badge. This caches badged icons per network per |icon_type|.
gfx::ImageSkia GetImageForNetwork(const chromeos::NetworkState* network,
                                  IconType icon_type);

// Gets the image for a connecting network type.
gfx::ImageSkia GetImageForConnectingNetwork(IconType icon_type,
                                            const std::string& network_type);

// Gets the image for a disconnected network type.
gfx::ImageSkia GetImageForDisconnectedNetwork(IconType icon_type,
                                              const std::string& network_type);

// Returns the label for |network| based on |icon_type|. |network| can be NULL.
base::string16 GetLabelForNetwork(const chromeos::NetworkState* network,
                                  IconType icon_type);

// Updates and returns the appropriate message id if the cellular network
// is uninitialized.
int GetCellularUninitializedMsg();

}  // namespace network_icon
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_H_
