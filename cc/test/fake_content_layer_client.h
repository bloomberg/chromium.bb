// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_
#define CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "cc/layers/content_layer_client.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/rect.h"

namespace cc {

class FakeContentLayerClient : public cc::ContentLayerClient {
 public:
  FakeContentLayerClient();
  virtual ~FakeContentLayerClient();

  virtual void PaintContents(SkCanvas* canvas,
                             gfx::Rect rect,
                             gfx::RectF* opaque_rect) OVERRIDE;
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}

  void set_paint_all_opaque(bool opaque) { paint_all_opaque_ = opaque; }

  void add_draw_rect(gfx::Rect rect, const SkPaint& paint) {
    draw_rects_.push_back(std::make_pair(rect, paint));
  }

 private:
  typedef std::vector<std::pair<gfx::Rect, SkPaint> > RectPaintVector;

  bool paint_all_opaque_;
  RectPaintVector draw_rects_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CONTENT_LAYER_CLIENT_H_
