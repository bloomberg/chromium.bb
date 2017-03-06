// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_record.h"

#include "cc/paint/paint_canvas.h"

namespace cc {

void PaintRecord::playback(PaintCanvas* canvas) {
  playback(canvas->canvas_);
}

}  // namespace cc
