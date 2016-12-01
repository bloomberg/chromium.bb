// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

enum LayoutConstant {
  // The padding on the left, right, and bottom of the avatar icon.
  AVATAR_ICON_PADDING,

  // Vertical offset from top of content to the top of find bar.
  FIND_BAR_TOOLBAR_OVERLAP,

  // The thickness of the location bar's border.
  LOCATION_BAR_BORDER_THICKNESS,

  // The vertical padding between the edge of a location bar bubble and its
  // contained text.
  LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING,

  // The additional vertical padding of a bubble.
  LOCATION_BAR_BUBBLE_VERTICAL_PADDING,

  // The vertical inset to apply to the bounds of a location bar bubble's anchor
  // view, to bring the bubble closer to the anchor.  This compensates for the
  // space between the bottoms of most such views and the visible bottoms of the
  // images inside.
  LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET,

  // The height to be occupied by the LocationBar.
  LOCATION_BAR_HEIGHT,

  // Space between items in the location bar, as well as between items and the
  // edges.
  LOCATION_BAR_HORIZONTAL_PADDING,

  // Space between the location bar edge and contents.
  LOCATION_BAR_VERTICAL_PADDING,

  // The font size to use in the location bar and omnibox dropdown, in pixels.
  OMNIBOX_FONT_PIXEL_SIZE,

  // The amount of overlap between the last tab and the new tab button.
  TABSTRIP_NEW_TAB_BUTTON_OVERLAP,

  // The amount of overlap between two adjacent tabs.
  TABSTRIP_TAB_OVERLAP,

  // The horizontal space between a tab's favicon and its title.
  TAB_FAVICON_TITLE_SPACING,

  // The height of a tab, including outer strokes.  In non-100% scales this is
  // slightly larger than the apparent height of the tab, as the top stroke is
  // drawn as a 1-px line flush with the bottom of the tab's topmost DIP.
  TAB_HEIGHT,

  // Width available for content inside a pinned tab.
  TAB_PINNED_CONTENT_WIDTH,

  // Padding inside toolbar button, between its border and image.
  TOOLBAR_BUTTON_PADDING,

  // Additional horizontal padding between the elements in the toolbar.
  TOOLBAR_ELEMENT_PADDING,

  // Padding between the right edge of the location bar and the left edge of the
  // app menu icon when the browser actions container is not present.
  TOOLBAR_LOCATION_BAR_RIGHT_PADDING,

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
