// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/frame/custom_frame_view_mus.h"

#include "ui/compositor/paint_cache.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"

namespace ash {
namespace mus {

CustomFrameViewMus::CustomFrameViewMus(
    views::Widget* widget,
    ImmersiveFullscreenControllerDelegate* immersive_delegate,
    bool enable_immersive)
    : CustomFrameViewAsh(widget, immersive_delegate, enable_immersive) {}

CustomFrameViewMus::~CustomFrameViewMus() {}

void CustomFrameViewMus::OnPaint(gfx::Canvas* canvas) {
  canvas->Save();
  CustomFrameViewAsh::OnPaint(canvas);
  canvas->Restore();

  // The client app draws the client area. Make ours totally transparent so
  // we only see the client app's client area.
  canvas->FillRect(GetBoundsForClientView(), SK_ColorBLACK,
                   SkBlendMode::kClear);
}

void CustomFrameViewMus::PaintChildren(const ui::PaintContext& context) {
  CustomFrameViewAsh::PaintChildren(context);
  // The client app draws the client area. Make ours totally transparent so
  // we only see the client apps client area.
  ui::PaintRecorder recorder(context, size(), &paint_cache_);
  recorder.canvas()->FillRect(GetBoundsForClientView(), SK_ColorBLACK,
                              SkBlendMode::kClear);
}

}  // namespace mus
}  // namespace ash
