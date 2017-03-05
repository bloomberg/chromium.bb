// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_INFO_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_INFO_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class ImageSkia;
}

namespace ash {

// Includes information necessary about a network for displaying the appropriate
// UI to the user.
struct NetworkInfo {
  enum class Type { UNKNOWN, WIFI, TETHER, CELLULAR };

  NetworkInfo();
  NetworkInfo(const std::string& guid);
  ~NetworkInfo();

  std::string guid;
  base::string16 label;
  base::string16 tooltip;
  gfx::ImageSkia image;
  bool disable;
  bool highlight;
  bool connected;
  bool connecting;
  Type type;
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_NETWORK_NETWORK_INFO_H_
