// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/shelf/wm_shelf_util.h"

namespace ash {
namespace wm {

bool IsHorizontalAlignment(wm::ShelfAlignment alignment) {
  return alignment == wm::SHELF_ALIGNMENT_BOTTOM ||
         alignment == wm::SHELF_ALIGNMENT_BOTTOM_LOCKED;
}

}  // namespace wm
}  // namespace ash
