// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_WORKLET_INPUT_H_
#define CC_PAINT_PAINT_WORKLET_INPUT_H_

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

class CC_EXPORT PaintWorkletInput
    : public base::RefCountedThreadSafe<PaintWorkletInput> {
 public:
  virtual gfx::SizeF GetSize() const = 0;
  virtual int WorkletId() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<PaintWorkletInput>;
  virtual ~PaintWorkletInput() = default;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_WORKLET_INPUT_H_
