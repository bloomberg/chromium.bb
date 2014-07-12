// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_H_
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_H_

#include <string>

#include "ash/ash_export.h"
#include "base/strings/string16.h"
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
ASH_EXPORT gfx::ImageSkia GetImageForNetwork(
    const chromeos::NetworkState* network,
    IconType icon_type);

// Similar to GetImageForNetwork but returns the cached image url based on
// |scale_factor| instead.
ASH_EXPORT std::string GetImageUrlForNetwork(
    const chromeos::NetworkState* network,
    IconType icon_type,
    float scale_factor);

// Gets the fulls strength image for a connected network type.
ASH_EXPORT gfx::ImageSkia GetImageForConnectedNetwork(
    IconType icon_type,
    const std::string& network_type);

// Gets the image for a connecting network type.
ASH_EXPORT gfx::ImageSkia GetImageForConnectingNetwork(
    IconType icon_type,
    const std::string& network_type);

// Gets the image for a disconnected network type.
ASH_EXPORT gfx::ImageSkia GetImageForDisconnectedNetwork(
    IconType icon_type,
    const std::string& network_type);

// Returns the label for |network| based on |icon_type|. |network| can be NULL.
ASH_EXPORT base::string16 GetLabelForNetwork(
    const chromeos::NetworkState* network,
    IconType icon_type);

// Updates and returns the appropriate message id if the cellular network
// is uninitialized.
ASH_EXPORT int GetCellularUninitializedMsg();

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

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_H_
