// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WINDOW_PROPERTY_H_
#define ASH_COMMON_WM_WINDOW_PROPERTY_H_

namespace ash {

enum class WmWindowProperty {
  // Type bool.
  SNAP_CHILDREN_TO_PIXEL_BOUNDARY,

  // Type bool.
  ALWAYS_ON_TOP,

  // Type int.
  SHELF_ID,

  // Type int. See aura::client::kTopViewInset for details.
  TOP_VIEW_INSET,

  // Type bool. See aura::client:kExcludeFromMruKey for details.
  EXCLUDE_FROM_MRU,
};

}  // namespace ash

#endif  // ASH_COMMON_WM_WINDOW_PROPERTY_H_
