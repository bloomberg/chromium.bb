// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_instance_mode.h"

#include "base/logging.h"

namespace arc {

std::ostream& operator<<(std::ostream& os, ArcInstanceMode mode) {
#define MAP_MODE(name)        \
  case ArcInstanceMode::name: \
    return os << #name

  switch (mode) {
    MAP_MODE(MINI_INSTANCE);
    MAP_MODE(FULL_INSTANCE);
  }
#undef MAP_MODE

  // Some compilers report an error even if all values of an enum-class are
  // covered exhaustively in a switch statement.
  NOTREACHED() << "Invalid value " << static_cast<int>(mode);
  return os;
}

}  // namespace arc
