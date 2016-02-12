// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_
#define CHROME_BROWSER_UI_LAYOUT_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"

enum LayoutConstant {
  // Vertical offset from top of content to the top of find bar.
  FIND_BAR_TOOLBAR_OVERLAP,

  // Horizontal padding applied between items of icon-label views.
  ICON_LABEL_VIEW_INTERNAL_PADDING,

  // Additional horizontal padding applied on the trailing edge of icon-label
  // views.
  ICON_LABEL_VIEW_TRAILING_PADDING,

  // The thickness of the location bar's border.
  LOCATION_BAR_BORDER_THICKNESS,

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

  // Space between the location bar edge and contents.
  LOCATION_BAR_VERTICAL_PADDING,

  // The number of pixels in the omnibox dropdown border image interior to
  // the actual border.
  OMNIBOX_DROPDOWN_BORDER_INTERIOR,

  // The font size to use in the location bar and omnibox dropdown, in pixels.
  OMNIBOX_FONT_PIXEL_SIZE,

  // The amount of overlap between the last tab and the new tab button.
  TABSTRIP_NEW_TAB_BUTTON_OVERLAP,

  // The amount of overlap between two adjacent tabs.
  TABSTRIP_TAB_OVERLAP,

  // The vertical overlap of the tabstrip atop the toolbar.
  TABSTRIP_TOOLBAR_OVERLAP,

  // The amount by which the tab close button should overlap the trailing
  // padding region after the tab's contents region.
  TAB_CLOSE_BUTTON_TRAILING_PADDING_OVERLAP,

  // The horizontal space between a tab's favicon and its title.
  TAB_FAVICON_TITLE_SPACING,

  // The height of a tab, including outer strokes.  In non-100% scales this is
  // slightly larger than the apparent height of the tab, as the top stroke is
  // drawn as a 1-px line flush with the bottom of the tab's topmost DIP.
  TAB_HEIGHT,

  // Width available for content inside a pinned tab.
  TAB_PINNED_CONTENT_WIDTH,

  // Height of the region atop the tab bounds that is normally excluded when
  // calculating the tab hit test region or considering where to paint a
  // custom theme background.  (Based on theme behavior it seems like at some
  // point this was added to the area above the top of the tab images, and we
  // now need to subtract it back off to compensate.)
  TAB_TOP_EXCLUSION_HEIGHT,

  // Non-ash uses a rounded content area with no shadow in the assets.
  // Ash doesn't use a rounded content area and its top edge has an extra
  // shadow.
  TOOLBAR_CONTENT_SHADOW_HEIGHT,
  TOOLBAR_CONTENT_SHADOW_HEIGHT_ASH,

  // Additional horizontal padding between the elements in the toolbar.
  TOOLBAR_ELEMENT_PADDING,

  // Padding between the right edge of the location bar and the left edge of the
  // app menu icon when the browser actions container is not present.
  TOOLBAR_LOCATION_BAR_RIGHT_PADDING,

  // The horizontal space between most items in the toolbar.
  TOOLBAR_STANDARD_SPACING,
};

enum LayoutInset {
  // The padding between the avatar icon and the frame border on the left, the
  // tabstrip on the right, and the toolbar on the bottom.
  AVATAR_ICON,

  // The padding above the top row and below the bottom row in the omnibox
  // dropdown.
  OMNIBOX_DROPDOWN,

  // In an omnibox dropdown row, the minimum distance between the icon and the
  // row edge.
  OMNIBOX_DROPDOWN_ICON,

  // In an omnibox dropdown row, the minimum distance between the text and the
  // row edge.
  OMNIBOX_DROPDOWN_TEXT,

  // The padding inside the tab bounds that defines the tab contents region.
  TAB,

  // The minimum padding of the toolbar.  The edge graphics have some built-in
  // spacing, shadowing, so this accounts for that as well.
  TOOLBAR,

  // The spacing between a ToolbarButton's image and its border.
  TOOLBAR_BUTTON,
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
