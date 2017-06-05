// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_ICON_H_
#define ASH_SYSTEM_NETWORK_NETWORK_ICON_H_

#include <string>

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
class NetworkState;
}

namespace ash {
namespace network_icon {

// Type of icon which dictates color theme and VPN badging
enum IconType {
  ICON_TYPE_TRAY,          // light icons with VPN badges
  ICON_TYPE_DEFAULT_VIEW,  // dark icons with VPN badges
  ICON_TYPE_LIST,          // dark icons without VPN badges; in-line status
  ICON_TYPE_MENU_LIST,     // dark icons without VPN badges; separate status
};

// Gets the image for provided |network|. |network| must not be NULL.
// |icon_type| determines the color theme and whether or not to show the VPN
// badge. This caches badged icons per network per |icon_type|.
ASH_EXPORT gfx::ImageSkia GetImageForNetwork(
    const chromeos::NetworkState* network,
    IconType icon_type);

// Gets an image for a Wi-Fi network, either full strength or strike-through
// based on |enabled|.
// TODO(estade): Expose SignalStrengthImageSource and use that instead.
ASH_EXPORT gfx::ImageSkia GetImageForWiFiEnabledState(
    bool enabled,
    IconType = ICON_TYPE_DEFAULT_VIEW);

// Gets the disconnected image for a cell network.
// TODO(estade): Expose SignalStrengthImageSource and use that instead.
ASH_EXPORT gfx::ImageSkia GetImageForDisconnectedCellNetwork();

// Gets the full strength image for a Wi-Fi network using |icon_color| for the
// main icon and |badge_color| for the badge.
ASH_EXPORT gfx::ImageSkia GetImageForNewWifiNetwork(SkColor icon_color,
                                                    SkColor badge_color);

// Returns the label for |network| based on |icon_type|. |network| cannot be
// nullptr.
ASH_EXPORT base::string16 GetLabelForNetwork(
    const chromeos::NetworkState* network,
    IconType icon_type);

// Updates and returns the appropriate message id if the mobile network
// is uninitialized.
ASH_EXPORT int GetMobileUninitializedMsg();

// Gets the correct icon and label for |icon_type|. Also sets |animating|
// based on whether or not the icon is animating (i.e. connecting).
ASH_EXPORT void GetDefaultNetworkImageAndLabel(IconType icon_type,
                                               gfx::ImageSkia* image,
                                               base::string16* label,
                                               bool* animating);

// Called when the list of networks changes. Retreives the list of networks
// from the global NetworkStateHandler instance and removes cached entries
// that are no longer in the list.
ASH_EXPORT void PurgeNetworkIconCache();

}  // namespace network_icon
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_ICON_H_
