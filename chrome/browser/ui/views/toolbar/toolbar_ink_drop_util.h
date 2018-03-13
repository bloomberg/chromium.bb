// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_

#include <memory>

#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_ripple.h"

namespace {

// Dimentions specific to the ink drop on the Browser's toolbar buttons when
// the Touch-optimized UI is being used.
constexpr int kTouchInkDropCornerRadius = 18;
constexpr gfx::Size kTouchInkDropHighlightSize{2 * kTouchInkDropCornerRadius,
                                               2 * kTouchInkDropCornerRadius};
constexpr gfx::Insets kTouchInkDropInsets{6};

}  // namespace

constexpr float kTouchToolbarInkDropVisibleOpacity = 0.06f;
constexpr float kTouchToolbarHighlightVisibleOpacity = 0.08f;

// The below utility functions are templated since we have two different types
// of buttons on the toolbar (ToolbarButton and AppMenuButton) which don't share
// the same base classes (ImageButton and MenuButton respectively), and these
// functions need to call into the base classes' default implementations when
// needed.
// TODO: Consider making ToolbarButton and AppMenuButton share a common base
// class https://crbug.com/819854.

// Creates the appropriate ink drop for the calling button. When the touch-
// optimized UI is not enabled, it uses the default implementation of the
// calling button's base class (the template argument BaseInkDropHostView).
// Otherwise, it uses an ink drop that shows a highlight on hover that is kept
// and combined with the ripple when the ripple is shown.
template <class BaseInkDropHostView>
std::unique_ptr<views::InkDrop> CreateToolbarInkDrop(
    BaseInkDropHostView* host_view) {
  if (!ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    return host_view->BaseInkDropHostView::CreateInkDrop();

  auto ink_drop =
      std::make_unique<views::InkDropImpl>(host_view, host_view->size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  ink_drop->SetShowHighlightOnHover(true);
  return ink_drop;
}

// Creates the appropriate ink drop ripple for the calling button. When the
// touch-optimized UI is not enabled, it uses the default implementation of the
// calling button's base class (the template argument BaseInkDropHostView).
// Otherwise, it uses a |FloodFillInkDropRipple|.
template <class BaseInkDropHostView>
std::unique_ptr<views::InkDropRipple> CreateToolbarInkDropRipple(
    const BaseInkDropHostView* host_view,
    const gfx::Point& center_point) {
  if (!ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    return host_view->BaseInkDropHostView::CreateInkDropRipple();

  return std::make_unique<views::FloodFillInkDropRipple>(
      host_view->size(), kTouchInkDropInsets, center_point,
      host_view->GetInkDropBaseColor(), host_view->ink_drop_visible_opacity());
}

// Creates the appropriate ink drop highlight for the calling button. When the
// touch-optimized UI is not enabled, it uses the default implementation of the
// calling button's base class (the template argument BaseInkDropHostView).
// Otherwise, it uses a kTouchInkDropHighlightSize circular highlight.
template <class BaseInkDropHostView>
std::unique_ptr<views::InkDropHighlight> CreateToolbarInkDropHighlight(
    const BaseInkDropHostView* host_view,
    const gfx::Point& center_point) {
  if (!ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    return host_view->BaseInkDropHostView::CreateInkDropHighlight();

  auto highlight = std::make_unique<views::InkDropHighlight>(
      kTouchInkDropHighlightSize, kTouchInkDropCornerRadius,
      gfx::PointF(center_point), host_view->GetInkDropBaseColor());
  highlight->set_visible_opacity(kTouchToolbarHighlightVisibleOpacity);
  return highlight;
}

// Creates the appropriate ink drop mask for the calling button. When the
// touch-optimized UI is not enabled, it uses the default implementation of the
// calling button's base class (the template argument BaseInkDropHostView).
// Otherwise, it uses a circular mask that has the same size as that of the
// highlight, which is needed to make the flood
// fill ripple fill a circle rather than a default square shape.
template <class BaseInkDropHostView>
std::unique_ptr<views::InkDropMask> CreateToolbarInkDropMask(
    const BaseInkDropHostView* host_view) {
  if (!ui::MaterialDesignController::IsTouchOptimizedUiEnabled())
    return host_view->BaseInkDropHostView::CreateInkDropMask();

  return std::make_unique<views::RoundRectInkDropMask>(
      host_view->size(), kTouchInkDropInsets, kTouchInkDropCornerRadius);
}

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
