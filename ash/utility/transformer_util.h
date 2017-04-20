// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TRANSFORMER_UTIL_H_
#define ASH_TRANSFORMER_UTIL_H_

#include "ash/ash_export.h"
#include "ui/display/display.h"

namespace gfx {
class Transform;
}

namespace ash {

// Creates rotation transform from |old_rotation| to |new_rotation| based on the
// |display| info.
ASH_EXPORT gfx::Transform CreateRotationTransform(
    display::Display::Rotation old_rotation,
    display::Display::Rotation new_rotation,
    const display::Display& display);

}  // namespace ash

#endif  // ASH_TRANSFORMER_UTIL_H_
