// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_constants.h"

#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace ash {

const int kPaddingFromRightEdgeOfScreenBottomAlignment = 7;
const int kPaddingFromBottomOfScreenBottomAlignment = 7;
const int kPaddingFromOuterEdgeOfLauncherVerticalAlignment = 8;
const int kPaddingFromInnerEdgeOfLauncherVerticalAlignment = 9;
const int kPaddingFromBottomOfScreenVerticalAlignment = 10;

// Padding used to position the system menu relative to the status area.
const int kBubblePaddingHorizontalBottom = 6;
const int kBubblePaddingHorizontalSide = 10;
const int kBubblePaddingVerticalBottom = 3;
const int kBubblePaddingVerticalSide = 15;

// Top inset of system tray bubble for bottom anchor alignment.
const int kTrayBubbleAnchorTopInsetBottomAnchor = 3;

const int kTrayImageItemHorizontalPaddingVerticalAlignment = 1;

// Size of tray items on the primary axis.
const int kTrayItemSize = 32;

const int kTrayImageItemPadding = 3;

const int kTrayLabelItemHorizontalPaddingBottomAlignment = 7;

// Vertical padding between status tray items when the shelf is vertical.
const int kTrayLabelItemVerticalPaddingVerticalAlignment = 4;

const int kTrayMenuBottomRowPadding = 3;
const int kTrayMenuBottomRowPaddingBetweenItems = -1;
const int kTrayMenuMinimumWidth = 300;
const int kTrayMenuMinimumWidthMd = 352;

const int kTrayPopupAutoCloseDelayInSeconds = 2;
const int kTrayPopupAutoCloseDelayForTextInSeconds = 5;
const int kTrayPopupPaddingHorizontal = 18;
const int kTrayPopupPaddingBetweenItems = 10;
const int kTrayPopupButtonEndMargin = 10;
const int kTrayPopupLabelHorizontalPadding = 4;
const int kTrayPopupItemMinHeight = 48;
const int kTrayPopupItemMinStartWidth = 48;
const int kTrayPopupItemMinEndWidth =
    kMenuIconSize + 2 * kTrayPopupButtonEndMargin;

const int kTrayDetailedViewTransitionDelayMs = 100;

const int kTrayPopupLabelRightPadding = 8;

const int kTrayPopupDetailsIconWidth = 25;
const int kTrayPopupDetailsLabelExtraLeftMargin = 8;
const SkColor kTrayPopupHoverBackgroundColor = SkColorSetRGB(0xe4, 0xe4, 0xe4);
const int kTrayPopupScrollSeparatorHeight = 15;
const int kTrayRoundedBorderRadius = 2;

const int kTrayToggleButtonWidth = 68;

const SkColor kBackgroundColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor kHoverBackgroundColor = SkColorSetRGB(0xf3, 0xf3, 0xf3);
const SkColor kPublicAccountUserCardTextColor = SkColorSetRGB(0x66, 0x66, 0x66);
const SkColor kPublicAccountUserCardNameColor = SK_ColorBLACK;

const SkColor kHeaderBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);

const SkColor kBorderDarkColor = SkColorSetRGB(0xaa, 0xaa, 0xaa);
const SkColor kBorderLightColor = SkColorSetRGB(0xeb, 0xeb, 0xeb);
const SkColor kButtonStrokeColor = SkColorSetRGB(0xdd, 0xdd, 0xdd);

const SkColor kHeaderTextColorNormal = SkColorSetARGB(0x7f, 0, 0, 0);
const SkColor kHeaderTextColorHover = SkColorSetARGB(0xd3, 0, 0, 0);

const int kTrayPopupMinWidth = 300;
const int kTrayPopupMaxWidth = 500;
const int kNotificationIconWidth = 40;
const int kNotificationButtonWidth = 32;
const int kTrayNotificationContentsWidth =
    kTrayPopupMinWidth - (kNotificationIconWidth + kNotificationButtonWidth +
                          (kTrayPopupPaddingHorizontal / 2) * 3);

const int kTrayIconSize = 16;
const int kTrayEdgePadding = 6;
const SkColor kTrayIconColor = SK_ColorWHITE;
const int kMenuIconSize = 20;
const SkColor kMenuIconColor = gfx::kChromeIconGrey;
const SkColor kMenuIconColorDisabled = SkColorSetA(gfx::kChromeIconGrey, 0x61);
const int kMenuButtonSize = 48;
const int kMenuSeparatorVerticalPadding = 4;
const int kMenuExtraMarginFromLeftEdge = 4;
const int kMenuEdgeEffectivePadding =
    kMenuExtraMarginFromLeftEdge + (kMenuButtonSize - kMenuIconSize) / 2;

const int kHitRegionPadding = 4;
const int kSeparatorWidth = 1;

const SkColor kMenuSeparatorColor = SkColorSetA(SK_ColorBLACK, 0x1F);

const SkColor kTrayPopupInkDropBaseColor = SK_ColorBLACK;
const float kTrayPopupInkDropRippleOpacity = 0.06f;
const float kTrayPopupInkDropHighlightOpacity = 0.08f;
const int kTrayPopupInkDropInset = 4;
const int kTrayPopupInkDropCornerRadius = 2;

const int kTrayPopupSystemInfoRowHeight = 40;

}  // namespace ash
