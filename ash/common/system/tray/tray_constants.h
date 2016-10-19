// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_CONSTANTS_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_CONSTANTS_H_

#include "ash/ash_export.h"

typedef unsigned int SkColor;

namespace ash {

extern const int kPaddingFromRightEdgeOfScreenBottomAlignment;
extern const int kPaddingFromBottomOfScreenBottomAlignment;
extern const int kPaddingFromOuterEdgeOfLauncherVerticalAlignment;
extern const int kPaddingFromInnerEdgeOfLauncherVerticalAlignment;
extern const int kPaddingFromBottomOfScreenVerticalAlignment;

extern const int kBubblePaddingHorizontalBottom;
extern const int kBubblePaddingHorizontalSide;
extern const int kBubblePaddingVerticalBottom;
extern const int kBubblePaddingVerticalSide;

extern const int kTrayBubbleAnchorTopInsetBottomAnchor;

extern const int kTrayImageItemHorizontalPaddingVerticalAlignment;

ASH_EXPORT extern const int kTrayItemSize;

extern const int kTrayLabelItemHorizontalPaddingBottomAlignment;
extern const int kTrayLabelItemVerticalPaddingVerticalAlignment;

extern const int kTrayMenuBottomRowPadding;
extern const int kTrayMenuBottomRowPaddingBetweenItems;

extern const int kTrayPopupAutoCloseDelayInSeconds;
extern const int kTrayPopupAutoCloseDelayForTextInSeconds;
extern const int kTrayPopupPaddingHorizontal;
extern const int kTrayPopupPaddingBetweenItems;
extern const int kTrayPopupTextSpacingVertical;
extern const int kTrayPopupUserCardVerticalPadding;

// Padding used to adjust the slider position in volume row and brightness
// row horizontally.
extern const int kTrayPopupSliderPaddingMD;

extern const int kTrayPopupDetailsIconWidth;
extern const int kTrayPopupDetailsLabelExtraLeftMargin;
extern const SkColor kTrayPopupHoverBackgroundColor;
extern const int kTrayPopupScrollSeparatorHeight;
extern const int kTrayRoundedBorderRadius;

extern const SkColor kBackgroundColor;
extern const SkColor kHoverBackgroundColor;
extern const SkColor kPublicAccountBackgroundColor;
extern const SkColor kPublicAccountUserCardTextColor;
extern const SkColor kPublicAccountUserCardNameColor;

extern const SkColor kHeaderBackgroundColor;

extern const SkColor kBorderDarkColor;
extern const SkColor kBorderLightColor;
extern const SkColor kButtonStrokeColor;

extern const SkColor kHeaderTextColorNormal;
extern const SkColor kHeaderTextColorHover;

extern const int kTrayPopupMinWidth;
extern const int kTrayPopupMaxWidth;
extern const int kNotificationIconWidth;
extern const int kNotificationButtonWidth;
extern const int kTrayNotificationContentsWidth;

// Extra padding used to adjust hitting region around tray items.
extern const int kHitRegionPadding;

// Color and width of a line used to separate tray items in the shelf.
extern const SkColor kSeparatorColor;
extern const int kSeparatorWidth;

// The color and height of the horizontal separator used in the material design
// system menu (i.e., the vertical line used to separate elements horizontally).
extern const SkColor kHorizontalSeparatorColor;
extern const int kHorizontalSeparatorHeight;

// The size and foreground color of the icons appearing in the material design
// system tray.
extern const int kTrayIconSize;
extern const SkColor kTrayIconColor;

// The size and foreground color of the icons appearing in the material design
// system menu.
extern const int kMenuIconSize;
extern const SkColor kMenuIconColor;
extern const SkColor kMenuIconColorDisabled;
// The size of buttons in the system menu.
extern const int kMenuButtonSize;
// The vertical padding for the system menu separator.
extern const int kMenuSeparatorVerticalPadding;
// The horizontal padding for the system menu separator.
extern const int kMenuExtraMarginFromLeftEdge;

enum TrayConstant {
  // A legacy height value used in non-MD calculations for applying additional
  // borders on tray items.
  TRAY_ITEM_HEIGHT_LEGACY,

  // Padding between items in the status tray area.
  TRAY_SPACING,

  // Padding between the edge of shelf and the item in status tray area.
  TRAY_PADDING_FROM_EDGE_OF_SHELF,

  // The height of the rows in the system tray menu.
  TRAY_POPUP_ITEM_HEIGHT,

  // The width and height of the virtual keyboard button in the status tray
  // area. For non-MD, adjustments are made to the button dimensions based on
  // the shelf orientation, so this constant does not specify the true
  // user-visible button bounds.
  VIRTUAL_KEYBOARD_BUTTON_SIZE,

  // The icon size of opt-in IME menu tray.
  TRAY_IME_MENU_ICON,

  // Extra padding used beside a single icon in the tray area of the shelf.
  TRAY_IMAGE_ITEM_PADDING,
};

int GetTrayConstant(TrayConstant constant);

namespace test {
const int kSettingsTrayItemViewId = 10000;
const int kAccessibilityTrayItemViewId = 10001;
}  // namespace test

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_CONSTANTS_H_
