// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_LAYOUT_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"

enum LayoutConstant {
  // Additional horizontal padding applied on the trailing edge of icon-label
  // views.
  ICON_LABEL_VIEW_TRAILING_PADDING,

  // The horizontal space between the edge and a bubble.
  LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING,

  // The additional vertical padding of a bubble.
  LOCATION_BAR_BUBBLE_VERTICAL_PADDING,

  // The height to be occupied by the LocationBar. For
  // MaterialDesignController::NON_MATERIAL the height is determined from image
  // assets.
  LOCATION_BAR_HEIGHT,

  // Space between items in the location bar, as well as between items and the
  // edges.
  LOCATION_BAR_HORIZONTAL_PADDING,

  // The vertical padding of items in the location bar.
  LOCATION_BAR_VERTICAL_PADDING,

  // The number of pixels in the omnibox dropdown border image interior to
  // the actual border.
  OMNIBOX_DROPDOWN_BORDER_INTERIOR,

  // The font size to use in the location bar and omnibox dropdown, in pixels.
  OMNIBOX_FONT_PIXEL_SIZE,

  // Non-ash uses a rounded content area with no shadow in the assets.
  // Ash doesn't use a rounded content area and its top edge has an extra
  // shadow.
  TOOLBAR_VIEW_CONTENT_SHADOW_HEIGHT,
  TOOLBAR_VIEW_CONTENT_SHADOW_HEIGHT_ASH,

  // Additional horizontal padding between the elements in the toolbar.
  TOOLBAR_VIEW_ELEMENT_PADDING,

  // Padding between the right-edge of the location bar and browser actions.
  TOOLBAR_VIEW_LOCATION_BAR_RIGHT_PADDING,

  // The horizontal space between most items.
  TOOLBAR_VIEW_STANDARD_SPACING,
};

enum LayoutInset {
  // In an omnibox dropdown row, the minimum distance between the icon and the
  // row edge.
  OMNIBOX_DROPDOWN_ICON,

  // In an omnibox dropdown row, the minimum distance between the text and the
  // row edge.
  OMNIBOX_DROPDOWN_TEXT,

  // The spacing between a ToolbarButton's image and its border.
  TOOLBAR_BUTTON,

  // The minimum padding of the toolbar.  The edge graphics have some built-in
  // spacing, shadowing, so this accounts for that as well.
  TOOLBAR_VIEW,
};

int GetLayoutConstant(LayoutConstant constant);
gfx::Insets GetLayoutInsets(LayoutInset inset);

#endif  // CHROME_BROWSER_UI_VIEWS_LAYOUT_CONSTANTS_H_
