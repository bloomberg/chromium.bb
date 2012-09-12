// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_constants.h"

#include "third_party/skia/include/core/SkColor.h"

namespace ash {

const int kPaddingFromRightEdgeOfScreenBottomAlignment = 15;
const int kPaddingFromBottomOfScreenBottomAlignment = 9;
const int kPaddingFromOuterEdgeOfLauncherVerticalAlignment = 8;
const int kPaddingFromInnerEdgeOfLauncherVerticalAlignment = 9;
const int kPaddingFromBottomOfScreenVerticalAlignment = 10;

const int kTrayImageItemHorizontalPaddingBottomAlignment = 1;
const int kTrayImageItemHorizontalPaddingVerticalAlignment = 1;
const int kTrayImageItemVerticalPaddingVerticalAlignment = 1;

const int kTrayLabelItemHorizontalPaddingBottomAlignment = 7;
const int kTrayLabelItemVerticalPaddingVeriticalAlignment = 4;

const int kTrayPopupAutoCloseDelayInSeconds = 2;
const int kTrayPopupAutoCloseDelayForTextInSeconds = 5;
const int kTrayPopupPaddingHorizontal = 18;
const int kTrayPopupPaddingBetweenItems = 10;
const int kTrayPopupTextSpacingVertical = 4;

const int kTrayPopupItemHeight = 48;
const int kTrayPopupDetailsIconWidth = 25;
const int kTrayRoundedBorderRadius = 2;
const int kTrayBarButtonWidth = 39;

const SkColor kBackgroundColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor kHoverBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);

const SkColor kHeaderBackgroundColorLight = SkColorSetRGB(0xf1, 0xf1, 0xf1);
const SkColor kHeaderBackgroundColorDark = SkColorSetRGB(0xe7, 0xe7, 0xe7);

const SkColor kBorderDarkColor = SkColorSetRGB(0xaa, 0xaa, 0xaa);
const SkColor kBorderLightColor = SkColorSetRGB(0xeb, 0xeb, 0xeb);
const SkColor kButtonStrokeColor = SkColorSetRGB(0xdd, 0xdd, 0xdd);

const SkColor kFocusBorderColor = SkColorSetRGB(64, 128, 250);

const SkColor kHeaderTextColorNormal = SkColorSetARGB(0x7f, 0, 0, 0);
const SkColor kHeaderTextColorHover = SkColorSetARGB(0xd3, 0, 0, 0);

const int kTrayPopupWidth = 300;
const int kNotificationIconWidth = 40;
const int kTrayNotificationContentsWidth =
    kTrayPopupWidth - kNotificationIconWidth*2 - kTrayPopupPaddingHorizontal*2;

}  // namespace ash
