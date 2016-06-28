// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_constants.h"

#include "ash/common/material_design/material_design_controller.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"

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

const int kTrayImageItemHorizontalPaddingBottomAlignment = 1;
const int kTrayImageItemHorizontalPaddingVerticalAlignment = 1;
const int kTrayImageItemVerticalPaddingVerticalAlignment = 1;

// Size of tray items on the primary axis.
const int kTrayItemSize = 32;

const int kTrayLabelItemHorizontalPaddingBottomAlignment = 7;

// Vertical padding between status tray items when the shelf is vertical.
const int kTrayLabelItemVerticalPaddingVerticalAlignment = 4;

const int kTrayMenuBottomRowPadding = 5;
const int kTrayMenuBottomRowPaddingBetweenItems = -1;

const int kTrayPopupAutoCloseDelayInSeconds = 2;
const int kTrayPopupAutoCloseDelayForTextInSeconds = 5;
const int kTrayPopupPaddingHorizontal = 18;
const int kTrayPopupPaddingBetweenItems = 10;
const int kTrayPopupTextSpacingVertical = 4;
const int kTrayPopupUserCardVerticalPadding = 10;

const int kTrayPopupItemHeight = 46;
const int kTrayPopupDetailsIconWidth = 25;
const int kTrayPopupDetailsLabelExtraLeftMargin = 8;
const SkColor kTrayPopupHoverBackgroundColor = SkColorSetRGB(0xe4, 0xe4, 0xe4);
const int kTrayPopupScrollSeparatorHeight = 15;
const int kTrayRoundedBorderRadius = 2;

const SkColor kBackgroundColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor kHoverBackgroundColor = SkColorSetRGB(0xf3, 0xf3, 0xf3);
const SkColor kPublicAccountBackgroundColor = SkColorSetRGB(0xf8, 0xe5, 0xb6);
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

const int kMessageCenterBubblePadding = 4;

int GetTrayConstant(TrayConstant constant) {
  const int kTrayItemHeightLegacy[] = {38, 38, kTrayItemSize};
  const int kTraySpacing[] = {4, 4, 8};
  const int kTrayPaddingFromEdgeOfShelf[] = {3, 3, 8};
  const int kVirtualKeyboardButtonSize[] = {39, 39, kTrayItemSize};

  const int mode = MaterialDesignController::GetMode();
  DCHECK(mode >= MaterialDesignController::NON_MATERIAL &&
         mode <= MaterialDesignController::MATERIAL_EXPERIMENTAL);

  switch (constant) {
    case TRAY_ITEM_HEIGHT_LEGACY:
      return kTrayItemHeightLegacy[mode];
    case TRAY_SPACING:
      return kTraySpacing[mode];
    case TRAY_PADDING_FROM_EDGE_OF_SHELF:
      return kTrayPaddingFromEdgeOfShelf[mode];
    case VIRTUAL_KEYBOARD_BUTTON_SIZE:
      return kVirtualKeyboardButtonSize[mode];
  }
  NOTREACHED();
  return 0;
}

}  // namespace ash
