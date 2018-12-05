// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_LAYER_TREE_PAINTER_H_
#define CC_PAINT_LAYER_TREE_PAINTER_H_

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

// TODO(xidachen): this class should be in its own file.
class CC_EXPORT PaintWorkletInput
    : public base::RefCountedThreadSafe<PaintWorkletInput> {
 public:
  virtual gfx::SizeF GetSize() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<PaintWorkletInput>;
  virtual ~PaintWorkletInput() = default;
};

class CC_EXPORT LayerTreePainter {
 public:
  virtual ~LayerTreePainter() {}

  // TODO(xidachen) add a PaintWorkletPaint function.
};

}  // namespace cc

#endif  // CC_PAINT_LAYER_TREE_PAINTER_H_
