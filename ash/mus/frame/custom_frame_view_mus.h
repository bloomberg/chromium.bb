// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_FRAME_CUSTOM_FRAME_VIEW_MUS_H_
#define ASH_MUS_FRAME_CUSTOM_FRAME_VIEW_MUS_H_

#include "ash/common/frame/custom_frame_view_ash.h"
#include "ui/compositor/paint_cache.h"

namespace ash {
namespace mus {

class CustomFrameViewMus : public CustomFrameViewAsh {
 public:
  CustomFrameViewMus(views::Widget* widget,
                     ImmersiveFullscreenControllerDelegate* immersive_delegate,
                     bool enable_immersive);
  ~CustomFrameViewMus() override;

  // CustomFrameViewAsh:
  void OnPaint(gfx::Canvas* canvas) override;
  void PaintChildren(const ui::PaintContext& context) override;

 private:
  ui::PaintCache paint_cache_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_FRAME_CUSTOM_FRAME_VIEW_MUS_H_
