// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/wm_shelf_util.h"

namespace ash {

bool IsHorizontalAlignment(ShelfAlignment alignment) {
  return alignment == SHELF_ALIGNMENT_BOTTOM ||
         alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED;
}

}  // namespace ash
