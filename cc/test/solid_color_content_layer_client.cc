// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/solid_color_content_layer_client.h"

#include <stddef.h>

#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/skia_util.h"

namespace cc {

gfx::Rect SolidColorContentLayerClient::PaintableRegion() {
  return gfx::Rect(size_);
}

scoped_refptr<DisplayItemList>
SolidColorContentLayerClient::PaintContentsToDisplayList(
    PaintingControlSetting painting_control) {
  auto display_list = base::MakeRefCounted<DisplayItemList>();
  display_list->StartPaint();
  display_list->push<SaveOp>();

  SkRect clip = gfx::RectToSkRect(PaintableRegion());
  display_list->push<ClipRectOp>(clip, SkClipOp::kIntersect, false);
  SkColor color = SK_ColorTRANSPARENT;
  display_list->push<DrawColorOp>(color, SkBlendMode::kSrc);

  if (border_size_ != 0) {
    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);
    flags.setColor(border_color_);
    display_list->push<DrawRectOp>(clip, flags);
  }

  PaintFlags flags;
  flags.setStyle(PaintFlags::kFill_Style);
  flags.setColor(color_);
  display_list->push<DrawRectOp>(clip.makeInset(border_size_, border_size_),
                                 flags);

  display_list->push<RestoreOp>();
  display_list->EndPaintOfUnpaired(PaintableRegion());
  display_list->Finalize();
  return display_list;
}

bool SolidColorContentLayerClient::FillsBoundsCompletely() const {
  return false;
}

size_t SolidColorContentLayerClient::GetApproximateUnsharedMemoryUsage() const {
  return 0;
}

}  // namespace cc
