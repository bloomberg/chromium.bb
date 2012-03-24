// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_constants.h"

#include "third_party/skia/include/core/SkColor.h"

namespace ash {

const int kTrayPaddingBetweenItems = 8;
const int kTrayPopupAutoCloseDelayInSeconds = 2;
const int kTrayPopupAutoCloseDelayForTextInSeconds = 5;
const int kTrayPopupPaddingHorizontal = 18;
const int kTrayPopupPaddingBetweenItems = 10;

const int kTrayPopupItemHeight = 48;
const int kTrayPopupDetailsIconWidth = 27;

const SkColor kBackgroundColor = SK_ColorWHITE;
const SkColor kHoverBackgroundColor = SkColorSetRGB(0xfb, 0xfc, 0xfb);

const SkColor kBorderDarkColor = SkColorSetRGB(120, 120, 120);
const SkColor kBorderLightColor = SkColorSetRGB(240, 240, 240);

const int kTrayPopupWidth = 300;

}  // namespace ash
