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

// Extra padding used beside a single icon in the tray area of the shelf.
extern const int kTrayImageItemPadding;

extern const int kTrayLabelItemHorizontalPaddingBottomAlignment;
extern const int kTrayLabelItemVerticalPaddingVerticalAlignment;

extern const int kTrayMenuBottomRowPadding;
extern const int kTrayMenuBottomRowPaddingBetweenItems;

// The minimum width of the tray menu.
extern const int kTrayMenuMinimumWidth;
extern const int kTrayMenuMinimumWidthMd;

extern const int kTrayPopupAutoCloseDelayInSeconds;
extern const int kTrayPopupAutoCloseDelayForTextInSeconds;
extern const int kTrayPopupPaddingHorizontal;
extern const int kTrayPopupPaddingBetweenItems;
extern const int kTrayPopupButtonEndMargin;
// The padding used on the left and right of labels. This applies to all labels
// in the system menu.
extern const int kTrayPopupLabelHorizontalPadding;

// The minimum/default height of the rows in the system tray menu.
extern const int kTrayPopupItemMinHeight;

// The width used for the first region of the row (which holds an image).
extern const int kTrayPopupItemMinStartWidth;

// The width used for the end region of the row (usually a more arrow).
extern const int kTrayPopupItemMinEndWidth;

// When transitioning between a detailed and a default view, this delay is used
// before the transition starts.
ASH_EXPORT extern const int kTrayDetailedViewTransitionDelayMs;

// Padding used on right side of labels to keep minimum distance to the next
// item. This applies to all labels in the system menu.
extern const int kTrayPopupLabelRightPadding;

extern const int kTrayPopupDetailsIconWidth;
extern const int kTrayPopupDetailsLabelExtraLeftMargin;
extern const SkColor kTrayPopupHoverBackgroundColor;
extern const int kTrayPopupScrollSeparatorHeight;
extern const int kTrayRoundedBorderRadius;

// The width of ToggleButton views including any border padding.
extern const int kTrayToggleButtonWidth;

extern const SkColor kBackgroundColor;
extern const SkColor kHoverBackgroundColor;
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

// Width of a line used to separate tray items in the shelf.
ASH_EXPORT extern const int kSeparatorWidth;

// The color of the separators used in the system menu.
extern const SkColor kMenuSeparatorColor;

// The size and foreground color of the icons appearing in the material design
// system tray.
extern const int kTrayIconSize;
extern const SkColor kTrayIconColor;

// The total visual padding at the start and end of the icon/label section
// of the tray.
extern const int kTrayEdgePadding;

// The size and foreground color of the icons appearing in the material design
// system menu.
extern const int kMenuIconSize;
extern const SkColor kMenuIconColor;
extern const SkColor kMenuIconColorDisabled;
// The size of buttons in the system menu.
ASH_EXPORT extern const int kMenuButtonSize;
// The vertical padding for the system menu separator.
extern const int kMenuSeparatorVerticalPadding;
// The horizontal padding for the system menu separator.
extern const int kMenuExtraMarginFromLeftEdge;
// The visual padding to the left of icons in the system menu.
extern const int kMenuEdgeEffectivePadding;

// The base color used for all ink drops in the system menu.
extern const SkColor kTrayPopupInkDropBaseColor;

// The opacity of the ink drop ripples for all ink drops in the system menu.
extern const float kTrayPopupInkDropRippleOpacity;

// The opacity of the ink drop ripples for all ink highlights in the system
// menu.
extern const float kTrayPopupInkDropHighlightOpacity;

// The inset applied to clickable surfaces in the system menu that do not have
// the ink drop filling the entire bounds.
extern const int kTrayPopupInkDropInset;

// The radius used to draw the corners of the rounded rect style ink drops.
extern const int kTrayPopupInkDropCornerRadius;

// The height of the system info row.
extern const int kTrayPopupSystemInfoRowHeight;

namespace test {
const int kSettingsTrayItemViewId = 10000;
const int kAccessibilityTrayItemViewId = 10001;
}  // namespace test

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_CONSTANTS_H_
