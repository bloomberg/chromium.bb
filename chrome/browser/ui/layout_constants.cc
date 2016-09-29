// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/layout_constants.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "ui/base/material_design/material_design_controller.h"

int GetLayoutConstant(LayoutConstant constant) {
  const int kFindBarVerticalOffset[] = {1, 6, 6};
  const int kLocationBarBorderThickness[] = {2, 1, 1};
  const int kLocationBarBubbleFontVerticalPadding[] = {1, 2, 4};
  const int kLocationBarBubbleHorizontalPadding[] = {1, 0, 0};
  const int kLocationBarBubbleVerticalPadding[] = {1, 3, 3};
  const int kLocationBarBubbleAnchorVerticalInset[] = {5, 6, 8};
  const int kLocationBarHeight[] = {27, 28, 32};
  const int kLocationBarHorizontalPadding[] = {3, 6, 6};
  const int kLocationBarVerticalPadding[] = {0, 1, 1};
  const int kOmniboxFontPixelSize[] = {16, 14, 14};
  const int kTabFaviconTitleSpacing[] = {4, 6, 6};
  const int kTabHeight[] = {29, 29, 33};
  const int kTabPinnedContentWidth[] = {25, 23, 23};
#if defined(OS_MACOSX)
  const int kTabstripNewTabButtonOverlap[] = {8, 5, 6};
  const int kTabstripTabOverlap[] = {19, 16, 16};
#else
  const int kTabstripNewTabButtonOverlap[] = {11, 5, 6};
  const int kTabstripTabOverlap[] = {26, 16, 16};
#endif
  const int kToolbarStandardSpacing[] = {3, 4, 8};
  const int kToolbarElementPadding[] = {0, 0, 8};
  const int kToolbarLocationBarRightPadding[] = {0, 4, 8};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (constant) {
    case FIND_BAR_TOOLBAR_OVERLAP:
      return kFindBarVerticalOffset[mode];
    case LOCATION_BAR_BORDER_THICKNESS:
      return kLocationBarBorderThickness[mode];
    case LOCATION_BAR_BUBBLE_FONT_VERTICAL_PADDING:
      return kLocationBarBubbleFontVerticalPadding[mode];
    case LOCATION_BAR_BUBBLE_HORIZONTAL_PADDING:
      return kLocationBarBubbleHorizontalPadding[mode];
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
  const int kAvatarLeftPadding[] = {2, 4, 4};
  const int kAvatarRightPadding[] = {-6, 4, 4};
  const int kAvatarBottomPadding[] = {2, 4, 4};
  const int kOmniboxDropdownIconPadding[] = {2, 4, 8};
  const int kOmniboxDropdownPadding[] = {3, 4, 4};
  const int kOmniboxDropdownTextPadding[] = {3, 3, 3};
  const int kTabBottomPadding[] = {2, 1, 1};
  const int kTabHorizontalPadding[] = {20, 16, 18};
  const int kTabTopPadding[] = {4, 1, 1};
  const int kToolbarBottomPadding[] = {5, 5, 5};
  const int kToolbarButtonPadding[] = {2, 6, 6};
  const int kToolbarLeftPadding[] = {3, 4, 8};
  const int kToolbarRightPadding[] = {2, 4, 8};
  const int kToolbarTopPadding[] = {5, 4, 4};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (inset) {
    case AVATAR_ICON:
      return gfx::Insets(0, kAvatarLeftPadding[mode],
                         kAvatarBottomPadding[mode], kAvatarRightPadding[mode]);
    case OMNIBOX_DROPDOWN:
      return gfx::Insets(kOmniboxDropdownPadding[mode], 0);
    case OMNIBOX_DROPDOWN_ICON:
      return gfx::Insets(kOmniboxDropdownIconPadding[mode], 0);
    case OMNIBOX_DROPDOWN_TEXT:
      return gfx::Insets(kOmniboxDropdownTextPadding[mode], 0);
    case TAB:
      return gfx::Insets(kTabTopPadding[mode], kTabHorizontalPadding[mode],
                         kTabBottomPadding[mode], kTabHorizontalPadding[mode]);
    case TOOLBAR:
      return gfx::Insets(kToolbarTopPadding[mode], kToolbarLeftPadding[mode],
                         kToolbarBottomPadding[mode],
                         kToolbarRightPadding[mode]);
    case TOOLBAR_BUTTON:
      return gfx::Insets(kToolbarButtonPadding[mode]);
  }
  NOTREACHED();
  return gfx::Insets();
}

gfx::Size GetLayoutSize(LayoutSize size) {
  const int kNewTabButtonWidth[] = {34, 36, 39};
  const int kNewTabButtonHeight[] = {18, 18, 21};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (size) {
    case NEW_TAB_BUTTON:
      return gfx::Size(kNewTabButtonWidth[mode], kNewTabButtonHeight[mode]);
  }
  NOTREACHED();
  return gfx::Size();
}
