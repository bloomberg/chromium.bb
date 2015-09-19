// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/layout_constants.h"

#include "base/logging.h"
#include "ui/base/resource/material_design/material_design_controller.h"

int GetLayoutConstant(LayoutConstant constant) {
  const int kIconLabelViewTrailingPadding[] = {2, 8, 8};
  const int kLocationBarBubbleHorizontalPadding[] = {1, 5, 5};
  const int kLocationBarBubbleVerticalPadding[] = {1, 3, 3};
  const int kLocationBarHeight[] = {0, 28, 32};
  const int kLocationBarHorizontalPadding[] = {3, 6, 6};
  const int kLocationBarVerticalPadding[] = {2, 2, 2};
  const int kOmniboxDropdownBorderInterior[] = {6, 0, 0};
  const int kOmniboxFontPixelSize[] = {16, 14, 14};
  const int kTabCloseButtonTrailingPaddingOverlap[] = {3, 3, 3};
  const int kTabFaviconTitleSpacing[] = {4, 4, 4};
  const int kTabMaximumTitleWidth[] = {175, 175, 175};
  const int kTabPinnedContentWidth[] = {25, 25, 25};
#if defined(OS_MACOSX)
  const int kTabstripTabOverlap[] = {19, 19, 19};
#else
  const int kTabstripTabOverlap[] = {26, 26, 26};
#endif
  const int kTabstripToolbarOverlap[] = {3, 3, 3};
  const int kTabstripTopShadowHeight[] = {3, 3, 3};
  const int kToolbarViewContentShadowHeight[] = {0, 0, 0};
  const int kToolbarViewContentShadowHeightAsh[] = {2, 0, 0};
  const int kToolbarViewElementPadding[] = {0, 0, 8};
  const int kToolbarViewLocationBarRightPadding[] = {0, 4, 8};
  const int kToolbarViewStandardSpacing[] = {3, 4, 8};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (constant) {
    case ICON_LABEL_VIEW_TRAILING_PADDING:
      return kIconLabelViewTrailingPadding[mode];
    case LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING:
      return kLocationBarBubbleHorizontalPadding[mode];
    case LOCATION_BAR_BUBBLE_VERTICAL_PADDING:
      return kLocationBarBubbleVerticalPadding[mode];
    case LOCATION_BAR_HEIGHT:
      return kLocationBarHeight[mode];
    case LOCATION_BAR_HORIZONTAL_PADDING:
      return kLocationBarHorizontalPadding[mode];
    case LOCATION_BAR_VERTICAL_PADDING:
      return kLocationBarVerticalPadding[mode];
    case OMNIBOX_DROPDOWN_BORDER_INTERIOR:
      return kOmniboxDropdownBorderInterior[mode];
    case OMNIBOX_FONT_PIXEL_SIZE:
      return kOmniboxFontPixelSize[mode];
    case TABSTRIP_TAB_OVERLAP:
      return kTabstripTabOverlap[mode];
    case TABSTRIP_TOOLBAR_OVERLAP:
      return kTabstripToolbarOverlap[mode];
    case TABSTRIP_TOP_SHADOW_HEIGHT:
      return kTabstripTopShadowHeight[mode];
    case TAB_CLOSE_BUTTON_TRAILING_PADDING_OVERLAP:
      return kTabCloseButtonTrailingPaddingOverlap[mode];
    case TAB_FAVICON_TITLE_SPACING:
      return kTabFaviconTitleSpacing[mode];
    case TAB_MAXIMUM_TITLE_WIDTH:
      return kTabMaximumTitleWidth[mode];
    case TAB_PINNED_CONTENT_WIDTH:
      return kTabPinnedContentWidth[mode];
    case TOOLBAR_VIEW_CONTENT_SHADOW_HEIGHT:
      return kToolbarViewContentShadowHeight[mode];
    case TOOLBAR_VIEW_CONTENT_SHADOW_HEIGHT_ASH:
      return kToolbarViewContentShadowHeightAsh[mode];
    case TOOLBAR_VIEW_ELEMENT_PADDING:
      return kToolbarViewElementPadding[mode];
    case TOOLBAR_VIEW_LOCATION_BAR_RIGHT_PADDING:
      return kToolbarViewLocationBarRightPadding[mode];
    case TOOLBAR_VIEW_STANDARD_SPACING:
      return kToolbarViewStandardSpacing[mode];
  }
  NOTREACHED();
  return 0;
}

gfx::Insets GetLayoutInsets(LayoutInset inset) {
  const int kOmniboxDropdownMinIconVerticalPadding[] = {2, 4, 8};
  const int kOmniboxDropdownMinTextVerticalPadding[] = {3, 4, 8};
  const int kTabBottomPadding[] = {2, 2, 2};
  const int kTabLeftPadding[] = {20, 20, 20};
  const int kTabRightPadding[] = {20, 20, 20};
  const int kTabTopPadding[] = {4, 4, 4};
  const int kToolbarButtonBorderInset[] = {2, 6, 6};
  const int kToolbarViewBottomVerticalPadding[] = {5, 5, 5};
  const int kToolbarViewTopVerticalPadding[] = {5, 4, 4};
  const int kToolbarViewLeftEdgeSpacing[] = {3, 4, 8};
  const int kToolbarViewRightEdgeSpacing[] = {2, 4, 8};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (inset) {
    case OMNIBOX_DROPDOWN_ICON: {
      const int padding = kOmniboxDropdownMinIconVerticalPadding[mode];
      return gfx::Insets(padding, 0, padding, 0);
    }
    case OMNIBOX_DROPDOWN_TEXT: {
      const int padding = kOmniboxDropdownMinTextVerticalPadding[mode];
      return gfx::Insets(padding, 0, padding, 0);
    }
    case TAB:
      return gfx::Insets(kTabTopPadding[mode], kTabLeftPadding[mode],
                         kTabBottomPadding[mode], kTabRightPadding[mode]);
    case TOOLBAR_BUTTON: {
      const int inset = kToolbarButtonBorderInset[mode];
      return gfx::Insets(inset, inset, inset, inset);
    }
    case TOOLBAR_VIEW:
      return gfx::Insets(kToolbarViewTopVerticalPadding[mode],
                         kToolbarViewLeftEdgeSpacing[mode],
                         kToolbarViewBottomVerticalPadding[mode],
                         kToolbarViewRightEdgeSpacing[mode]);
  }
  NOTREACHED();
  return gfx::Insets();
}
