// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_WINDOW_PROPERTY_H_
#define ASH_WM_COMMON_WM_WINDOW_PROPERTY_H_

namespace ash {
namespace wm {

enum class WmWindowProperty {
  // Type bool.
  SNAP_CHILDREN_TO_PIXEL_BOUDARY,

  // Type bool.
  ALWAYS_ON_TOP,

  // Type int.
  SHELF_ID,
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_WINDOW_PROPERTY_H_
