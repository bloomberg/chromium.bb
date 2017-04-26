// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_SKCANVAS_H_
#define CC_TEST_TEST_SKCANVAS_H_

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {

class SaveCountingCanvas : public SkNoDrawCanvas {
 public:
  SaveCountingCanvas();

  // Note: getSaveLayerStrategy is used as "willSave", as willSave
  // is not always called.
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override;
  void willRestore() override;
  void onDrawRect(const SkRect& rect, const SkPaint& paint) override;

  int save_count_ = 0;
  int restore_count_ = 0;
  SkRect draw_rect_;
  SkPaint paint_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_SKCANVAS_H_
