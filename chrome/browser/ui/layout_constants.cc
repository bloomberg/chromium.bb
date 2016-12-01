// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/layout_constants.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/base/material_design/material_design_controller.h"

int GetLayoutConstant(LayoutConstant constant) {
  const int kFindBarVerticalOffset[] = {6, 6};
  const int kLocationBarBorderThickness[] = {1, 1};
  const int kLocationBarBubbleFontVerticalPadding[] = {2, 4};
  const int kLocationBarBubbleVerticalPadding[] = {3, 3};
  const int kLocationBarBubbleAnchorVerticalInset[] = {6, 8};
  const int kLocationBarHeight[] = {28, 32};
  const int kLocationBarHorizontalPadding[] = {6, 6};
  const int kLocationBarVerticalPadding[] = {1, 1};
  const int kOmniboxFontPixelSize[] = {14, 14};
  const int kTabFaviconTitleSpacing[] = {6, 6};
  const int kTabHeight[] = {29, 33};
  const int kTabPinnedContentWidth[] = {23, 23};
  const int kTabstripNewTabButtonOverlap[] = {5, 6};
  const int kTabstripTabOverlap[] = {16, 16};
  const int kToolbarStandardSpacing[] = {4, 8};
  const int kToolbarElementPadding[] = {0, 8};
  const int kToolbarLocationBarRightPadding[] = {4, 8};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (constant) {
    case AVATAR_ICON_PADDING:
      return 4;
    case FIND_BAR_TOOLBAR_OVERLAP:
      return kFindBarVerticalOffset[mode];
    case LOCATION_BAR_BORDER_THICKNESS:
      return kLocationBarBorderThickness[mode];
    case LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING:
      return kLocationBarBubbleFontVerticalPadding[mode];
    case LOCATION_BAR_BUBBLE_VERTICAL_PADDING:
      return kLocationBarBubbleVerticalPadding[mode];
    case LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET:
      if (ui::MaterialDesignController::IsSecondaryUiMaterial())
        return 1;
      return kLocationBarBubbleAnchorVerticalInset[mode];
    case LOCATION_BAR_HEIGHT:
      return kLocationBarHeight[mode];
    case LOCATION_BAR_HORIZONTAL_PADDING:
      return kLocationBarHorizontalPadding[mode];
    case LOCATION_BAR_VERTICAL_PADDING:
      return kLocationBarVerticalPadding[mode];
    case OMNIBOX_FONT_PIXEL_SIZE:
      return kOmniboxFontPixelSize[mode];
    case TABSTRIP_NEW_TAB_BUTTON_OVERLAP:
      return kTabstripNewTabButtonOverlap[mode];
    case TABSTRIP_TAB_OVERLAP:
      return kTabstripTabOverlap[mode];
    case TAB_FAVICON_TITLE_SPACING:
      return kTabFaviconTitleSpacing[mode];
    case TAB_HEIGHT:
      return kTabHeight[mode];
    case TAB_PINNED_CONTENT_WIDTH:
      return kTabPinnedContentWidth[mode];
    case TOOLBAR_BUTTON_PADDING:
      return 6;
    case TOOLBAR_ELEMENT_PADDING:
      return kToolbarElementPadding[mode];
    case TOOLBAR_LOCATION_BAR_RIGHT_PADDING:
      return kToolbarLocationBarRightPadding[mode];
    case TOOLBAR_STANDARD_SPACING:
      return kToolbarStandardSpacing[mode];
  }
  NOTREACHED();
  return 0;
}

gfx::Insets GetLayoutInsets(LayoutInset inset) {
  switch (inset) {
    case TAB: {
      const bool hybrid = ui::MaterialDesignController::GetMode() ==
                          ui::MaterialDesignController::MATERIAL_HYBRID;
      return gfx::Insets(1, hybrid ? 18 : 16);
    }
  }
  NOTREACHED();
  return gfx::Insets();
}

gfx::Size GetLayoutSize(LayoutSize size) {
  const int kNewTabButtonWidth[] = {36, 39};
  const int kNewTabButtonHeight[] = {18, 21};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (size) {
    case NEW_TAB_BUTTON:
      return gfx::Size(kNewTabButtonWidth[mode], kNewTabButtonHeight[mode]);
  }
  NOTREACHED();
  return gfx::Size();
}
