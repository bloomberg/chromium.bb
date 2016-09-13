// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_WINDOW_TREE_HOST_INIT_PARAMS_H_
#define ASH_HOST_WINDOW_TREE_HOST_INIT_PARAMS_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

struct ASH_EXPORT AshWindowTreeHostInitParams {
  gfx::Rect initial_bounds;
  bool offscreen = false;
};

}  // namespace ash

#endif  // ASH_HOST_WINDOW_TREE_HOST_INIT_PARAMS_H_
