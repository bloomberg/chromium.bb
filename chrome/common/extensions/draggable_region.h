// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_DRAGGABLE_REGION_H_
#define CHROME_COMMON_EXTENSIONS_DRAGGABLE_REGION_H_

#include "ui/gfx/rect.h"

namespace extensions {

struct DraggableRegion {
  std::string label;
  gfx::Rect bounds;
  gfx::Rect clip;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_DRAGGABLE_REGION_H_
