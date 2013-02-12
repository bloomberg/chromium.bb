// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_H_
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_H_

#include <string>

#include "ui/gfx/image/image_skia.h"

namespace chromeos {
class NetworkState;
}

namespace ash {
namespace network_icon {

class AnimationObserver;

// Color theme based on icon background.
enum ResourceColorTheme {
  COLOR_DARK,
  COLOR_LIGHT,
};

// Get the image for the network associated with |service_path|.
// |color| determines the color theme. If the icon is animating (i.e the
// network is connecting) and |observer| is provided, it will be notified
// when the icon changes.
gfx::ImageSkia GetImageForNetwork(const chromeos::NetworkState* network,
                                  ResourceColorTheme color,
                                  AnimationObserver* observer);

}  // namespace network_icon
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_H_
