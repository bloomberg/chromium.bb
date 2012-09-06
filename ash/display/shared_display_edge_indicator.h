// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SHARED_DISPLAY_EDGE_INDICATOR_H_
#define ASH_DISPLAY_SHARED_DISPLAY_EDGE_INDICATOR_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/display.h"

namespace gfx {
class Rect;
}

namespace ui {
class ThrobAnimation;
}

namespace views {
class View;
class Widget;
}

namespace ash {
namespace internal {

// SharedDisplayEdgeIndicator is responsible for showing a window that indicates
// the edge that a window can be dragged into another display.
class ASH_EXPORT SharedDisplayEdgeIndicator : public ui::AnimationDelegate {
 public:
  SharedDisplayEdgeIndicator();
  virtual ~SharedDisplayEdgeIndicator();

  // Shows/Hides the indicator window. The |src_bounds| is for the window on
  // the source display, and the |dst_bounds| is for the window on the other
  // display.
  void Show(const gfx::Rect& src_bounds, const gfx::Rect& dst_bounds);
  void Hide();

  // ui::AnimationDelegate overrides:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

 private:
  // Used to show the displays' shared edge where a window can be moved across.
  // |src_widget_| is for the display where drag starts and |dst_widget_| is
  // for the other display.
  views::View* src_indicator_;
  views::View* dst_indicator_;

  // Used to transition the opacity.
  scoped_ptr<ui::ThrobAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(SharedDisplayEdgeIndicator);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_SHARED_DISPLAY_EDGE_INDICATOR_H_
