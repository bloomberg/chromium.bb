// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_constants.h"

#include "third_party/skia/include/core/SkColor.h"

namespace ash {

const int kTrayPopupAutoCloseDelayInSeconds = 2;
const int kTrayPopupAutoCloseDelayForTextInSeconds = 5;
const int kTrayPopupPaddingHorizontal = 18;
const int kTrayPopupPaddingBetweenItems = 10;

const int kTrayPopupItemHeight = 48;
const int kTrayPopupDetailsIconWidth = 27;
const int kTrayRoundedBorderRadius = 2;

const SkColor kBackgroundColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor kHoverBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);

const SkColor kBorderDarkColor = SkColorSetARGB(51, 0, 0, 0);
const SkColor kBorderLightColor = SkColorSetRGB(0xeb, 0xeb, 0xeb);

const int kTrayPopupWidth = 300;

}  // namespace ash
