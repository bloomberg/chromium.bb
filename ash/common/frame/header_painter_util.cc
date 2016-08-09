// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/frame/header_painter_util.h"

#include <algorithm>

#include "ash/common/wm_lookup.h"
#include "ash/common/wm_window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Radius of the header's top corners when the window is restored.
const int kTopCornerRadiusWhenRestored = 2;

// Distance between left edge of the window and the leftmost view.
const int kLeftViewXInset = 9;

// Space between the title text and the caption buttons.
const int kTitleCaptionButtonSpacing = 5;

// Space between window icon and title text.
const int kTitleIconOffsetX = 5;

// Space between window edge and title text, when there is no icon.
const int kTitleNoIconOffsetX = 8;

// In the pre-Ash era the web content area had a frame along the left edge, so
// user-generated theme images for the new tab page assume they are shifted
// right relative to the header.  Now that we have removed the left edge frame
// we need to copy the theme image for the window header from a few pixels
// inset to preserve alignment with the NTP image, or else we'll break a bunch
// of existing themes.  We do something similar on OS X for the same reason.
const int kThemeFrameImageInsetX = 5;

}  // namespace

namespace ash {

// static
int HeaderPainterUtil::GetTopCornerRadiusWhenRestored() {
  return kTopCornerRadiusWhenRestored;
}

// static
int HeaderPainterUtil::GetLeftViewXInset() {
  return kLeftViewXInset;
}

// static
int HeaderPainterUtil::GetThemeBackgroundXInset() {
  return kThemeFrameImageInsetX;
}

// static
gfx::Rect HeaderPainterUtil::GetTitleBounds(
    const views::View* left_view,
    const views::View* right_view,
    const gfx::FontList& title_font_list) {
  int x = left_view ? left_view->bounds().right() + kTitleIconOffsetX
                    : kTitleNoIconOffsetX;
  int height = title_font_list.GetHeight();
  // Floor when computing the center of |caption_button_container| and when
  // computing the center of the text.
  int y = std::max(0, (right_view->height() / 2) - (height / 2));
  int width = std::max(0, right_view->x() - kTitleCaptionButtonSpacing - x);
  return gfx::Rect(x, y, width, height);
}

// static
bool HeaderPainterUtil::CanAnimateActivation(views::Widget* widget) {
  // Do not animate the header if the parent (e.g.
  // kShellWindowId_DefaultContainer) is already animating. All of the
  // implementers of HeaderPainter animate activation by continuously painting
  // during the animation. This gives the parent's animation a slower frame
  // rate.
  // TODO(sky): Expose a better way to determine this rather than assuming the
  // parent is a toplevel container.
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget);
  // TODO(sky): GetParent()->GetLayer() is for mash until animations ported.
  if (!window || !window->GetParent() || !window->GetParent()->GetLayer())
    return true;

  ui::LayerAnimator* parent_layer_animator =
      window->GetParent()->GetLayer()->GetAnimator();
  return !parent_layer_animator->IsAnimatingProperty(
             ui::LayerAnimationElement::OPACITY) &&
         !parent_layer_animator->IsAnimatingProperty(
             ui::LayerAnimationElement::VISIBILITY);
}

}  // namespace ash
