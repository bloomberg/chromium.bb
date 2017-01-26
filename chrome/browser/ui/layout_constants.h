// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

enum LayoutConstant {
  // The vertical padding between the edge of a location bar bubble and its
  // contained text.
  LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING,

  // The vertical inset to apply to the bounds of a location bar bubble's anchor
  // view, to bring the bubble closer to the anchor.  This compensates for the
  // space between the bottoms of most such views and the visible bottoms of the
  // images inside.
  LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET,

  // The horizontal padding between location bar decorations as well as the
  // vertical and horizontal padding inside the border.
  LOCATION_BAR_ELEMENT_PADDING,

  // The height to be occupied by the LocationBar.
  LOCATION_BAR_HEIGHT,

  // The amount of overlap between the last tab and the new tab button.
  TABSTRIP_NEW_TAB_BUTTON_OVERLAP,

  // The height of a tab, including outer strokes.  In non-100% scales this is
  // slightly larger than the apparent height of the tab, as the top stroke is
  // drawn as a 1-px line flush with the bottom of the tab's topmost DIP.
  TAB_HEIGHT,

  // Additional horizontal padding between the elements in the toolbar.
  TOOLBAR_ELEMENT_PADDING,

  // The horizontal space between most items in the toolbar.
  TOOLBAR_STANDARD_SPACING,
};

enum LayoutInset {
  // The padding inside the tab bounds that defines the tab contents region.
  TAB,
};

enum LayoutSize {
  // The visible size of the new tab button; does not include any Fitts' Law
  // extensions.
  NEW_TAB_BUTTON,
};

int GetLayoutConstant(LayoutConstant constant);
gfx::Insets GetLayoutInsets(LayoutInset inset);
gfx::Size GetLayoutSize(LayoutSize size);

#endif  // CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
