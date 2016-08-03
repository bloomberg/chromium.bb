// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_POPUP_CONSTANTS_H_
#define CHROME_BROWSER_UI_AUTOFILL_POPUP_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace autofill {

// The size of the border around the entire results popup, in pixels.
const int kPopupBorderThickness = 1;

// Various colors used in the Autofill popup.
const SkColor kBorderColor = SkColorSetRGB(0xC7, 0xCA, 0xCE);
const SkColor kHoveredBackgroundColor = SkColorSetRGB(0xCD, 0xCD, 0xCD);
const SkColor kLabelTextColor = SkColorSetRGB(0x64, 0x64, 0x64);
constexpr SkColor kPopupBackground = SK_ColorWHITE;
constexpr SkColor kValueTextColor = SK_ColorBLACK;

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_POPUP_CONSTANTS_H_
