// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_UI_CONSTANTS_H_
#define ASH_ASSISTANT_UI_ASSISTANT_UI_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// Appearance.
constexpr int kPaddingDip = 14;
constexpr int kPreferredWidthDip = 640;
constexpr int kSpacingDip = 8;

// Typography.
constexpr SkColor kTextColorHint = SkColorSetA(SK_ColorBLACK, 0x42);
constexpr SkColor kTextColorPrimary = SkColorSetA(SK_ColorBLACK, 0xDE);

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_UI_CONSTANTS_H_
