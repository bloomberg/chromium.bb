// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_

#include <memory>

#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_ripple.h"
#include "ui/views/style/platform_style.h"

constexpr float kToolbarInkDropVisibleOpacity = 0.06f;
constexpr SkAlpha kToolbarButtonBackgroundAlpha = 32;

// The below utility functions are templated since we have two different types
// of buttons on the toolbar (ToolbarButton and AppMenuButton) which don't share
// the same base classes (ImageButton and MenuButton respectively), and these
// functions need to call into the base classes' default implementations when
// needed.
// TODO: Consider making ToolbarButton and AppMenuButton share a common base
// class https://crbug.com/819854.

// Creates insets for a host view so that when insetting from the host view
// the resulting mask or inkdrop has the desired inkdrop size.
gfx::Insets GetToolbarInkDropInsets(const views::View* host_view,
                                    const gfx::Insets& margin_insets);

// Set the highlight path to be used for inkdrops and focus rings.
void SetToolbarButtonHighlightPath(views::View* host_view,
                                   const gfx::Insets& margin_insets);

// Creates an ink drop that shows a highlight on hover that is kept and combined
// with the ripple when the ripple is shown.
template <class BaseInkDropHostView>
std::unique_ptr<views::InkDrop> CreateToolbarInkDrop(
    BaseInkDropHostView* host_view) {
  auto ink_drop =
      std::make_unique<views::InkDropImpl>(host_view, host_view->size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  ink_drop->SetShowHighlightOnHover(true);
  ink_drop->SetShowHighlightOnFocus(!views::PlatformStyle::kPreferFocusRings);

  return ink_drop;
}

// Creates the default inkdrop highlight but using the toolbar visible opacity.
std::unique_ptr<views::InkDropHighlight> CreateToolbarInkDropHighlight(
    const views::InkDropHostView* host_view);

// Returns the ink drop base color that should be used by all toolbar buttons.
SkColor GetToolbarInkDropBaseColor(const views::View* host_view);

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_INK_DROP_UTIL_H_
