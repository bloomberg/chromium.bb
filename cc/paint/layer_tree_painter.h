// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_LAYER_TREE_PAINTER_H_
#define CC_PAINT_LAYER_TREE_PAINTER_H_

#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT LayerTreePainter {
 public:
  virtual ~LayerTreePainter() {}

  // TODO(xidachen) add a PaintWorkletPaint function.
};

}  // namespace cc

#endif  // CC_PAINT_LAYER_TREE_PAINTER_H_
