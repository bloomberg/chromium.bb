// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/layout_constants.h"

#include "base/logging.h"
#include "ui/base/resource/material_design/material_design_controller.h"

int GetLayoutConstant(LayoutConstant constant) {
  const int kFindBarVerticalOffset[] = {1, 6, 6};
  const int kIconLabelViewInternalPadding[] = {3, 2, 2};
  const int kIconLabelViewTrailingPadding[] = {2, 8, 8};
  const int kLocationBarBubbleHorizontalPadding[] = {1, 5, 5};
  const int kLocationBarBubbleVerticalPadding[] = {1, 5, 5};
  const int kLocationBarHeight[] = {0, 28, 32};
  const int kLocationBarHorizontalPadding[] = {3, 6, 6};
  const int kLocationBarVerticalPadding[] = {2, 2, 2};
  const int kNewTabButtonWidth[] = {34, 34, 34};
  const int kOmniboxDropdownBorderInterior[] = {6, 0, 0};
  const int kOmniboxFontPixelSize[] = {16, 14, 14};
  const int kTabCloseButtonTrailingPaddingOverlap[] = {2, 2, 2};
  const int kTabFaviconTitleSpacing[] = {4, 4, 4};
  const int kTabMaximumTitleWidth[] = {175, 175, 175};
  const int kTabPinnedContentWidth[] = {25, 25, 25};
#if defined(OS_MACOSX)
  const int kTabstripNewTabButtonOverlap[] = {8, 8, 8};
  const int kTabstripTabOverlap[] = {19, 19, 19};
#else
  const int kTabstripNewTabButtonOverlap[] = {11, 11, 11};
  const int kTabstripTabOverlap[] = {26, 26, 26};
#endif
  const int kTabstripToolbarOverlap[] = {3, 3, 3};
  const int kToolbarContentShadowHeight[] = {0, 0, 0};
  const int kToolbarContentShadowHeightAsh[] = {2, 0, 0};
  const int kToolbarElementPadding[] = {0, 0, 8};
  const int kToolbarLocationBarRightPadding[] = {0, 4, 8};
  const int kToolbarStandardSpacing[] = {3, 4, 8};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (constant) {
    case FIND_BAR_TOOLBAR_OVERLAP:
      return kFindBarVerticalOffset[mode];
    case ICON_LABEL_VIEW_INTERNAL_PADDING:
      return kIconLabelViewInternalPadding[mode];
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
    case NEW_TAB_BUTTON_WIDTH:
      return kNewTabButtonWidth[mode];
    case OMNIBOX_DROPDOWN_BORDER_INTERIOR:
      return kOmniboxDropdownBorderInterior[mode];
    case OMNIBOX_FONT_PIXEL_SIZE:
      return kOmniboxFontPixelSize[mode];
    case TABSTRIP_NEW_TAB_BUTTON_OVERLAP:
      return kTabstripNewTabButtonOverlap[mode];
    case TABSTRIP_TAB_OVERLAP:
      return kTabstripTabOverlap[mode];
    case TABSTRIP_TOOLBAR_OVERLAP:
      return kTabstripToolbarOverlap[mode];
    case TAB_CLOSE_BUTTON_TRAILING_PADDING_OVERLAP:
      return kTabCloseButtonTrailingPaddingOverlap[mode];
    case TAB_FAVICON_TITLE_SPACING:
      return kTabFaviconTitleSpacing[mode];
    case TAB_MAXIMUM_TITLE_WIDTH:
      return kTabMaximumTitleWidth[mode];
    case TAB_PINNED_CONTENT_WIDTH:
      return kTabPinnedContentWidth[mode];
    case TOOLBAR_CONTENT_SHADOW_HEIGHT:
      return kToolbarContentShadowHeight[mode];
    case TOOLBAR_CONTENT_SHADOW_HEIGHT_ASH:
      return kToolbarContentShadowHeightAsh[mode];
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
  const int kTabBottomPadding[] = {2, 2, 2};
  const int kTabLeftPadding[] = {20, 20, 20};
  const int kTabRightPadding[] = {20, 20, 20};
  const int kTabTopPadding[] = {4, 4, 4};
  const int kToolbarBottomPadding[] = {5, 5, 5};
  const int kToolbarButtonPadding[] = {2, 6, 6};
  const int kToolbarLeftPadding[] = {3, 4, 8};
  const int kToolbarRightPadding[] = {2, 4, 8};
  const int kToolbarTopPadding[] = {5, 4, 4};

  const int mode = ui::MaterialDesignController::GetMode();
  switch (inset) {
    case AVATAR_ICON: {
      return gfx::Insets(0, kAvatarLeftPadding[mode],
                         kAvatarBottomPadding[mode], kAvatarRightPadding[mode]);
    }
    case OMNIBOX_DROPDOWN: {
      const int padding = kOmniboxDropdownPadding[mode];
      return gfx::Insets(padding, 0, padding, 0);
    }
    case OMNIBOX_DROPDOWN_ICON: {
      const int padding = kOmniboxDropdownIconPadding[mode];
      return gfx::Insets(padding, 0, padding, 0);
    }
    case OMNIBOX_DROPDOWN_TEXT: {
      const int padding = kOmniboxDropdownTextPadding[mode];
      return gfx::Insets(padding, 0, padding, 0);
    }
    case TAB:
      return gfx::Insets(kTabTopPadding[mode], kTabLeftPadding[mode],
                         kTabBottomPadding[mode], kTabRightPadding[mode]);
    case TOOLBAR:
      return gfx::Insets(kToolbarTopPadding[mode], kToolbarLeftPadding[mode],
                         kToolbarBottomPadding[mode],
                         kToolbarRightPadding[mode]);
    case TOOLBAR_BUTTON: {
      const int inset = kToolbarButtonPadding[mode];
      return gfx::Insets(inset, inset, inset, inset);
    }
  }
  NOTREACHED();
  return gfx::Insets();
}
