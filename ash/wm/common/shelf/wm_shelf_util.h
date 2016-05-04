// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_SHELF_WM_SHELF_UTIL_H_
#define ASH_WM_COMMON_SHELF_WM_SHELF_UTIL_H_

#include "ash/wm/common/ash_wm_common_export.h"
#include "ash/wm/common/shelf/wm_shelf_types.h"

namespace ash {
namespace wm {

// Returns true if the shelf |alignment| is horizontal.
ASH_WM_COMMON_EXPORT bool IsHorizontalAlignment(ShelfAlignment alignment);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_SHELF_WM_SHELF_UTIL_H_
