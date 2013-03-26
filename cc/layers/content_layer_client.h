// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_CONTENT_LAYER_CLIENT_H_
#define CC_LAYERS_CONTENT_LAYER_CLIENT_H_

#include "cc/base/cc_export.h"

class SkCanvas;

namespace gfx {
class Rect;
class RectF;
}

namespace cc {

class CC_EXPORT ContentLayerClient {
 public:
  virtual void PaintContents(SkCanvas* canvas,
                             gfx::Rect clip,
                             gfx::RectF* opaque) = 0;

  // Called by the content layer during the update phase.
  // If the client paints LCD text, it may want to invalidate the layer.
  virtual void DidChangeLayerCanUseLCDText() = 0;

 protected:
  virtual ~ContentLayerClient() {}
};

}  // namespace cc

#endif  // CC_LAYERS_CONTENT_LAYER_CLIENT_H_
