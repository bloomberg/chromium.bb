// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
#define ASH_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

namespace message_center_style {

constexpr SkColor kBackgroundColor = SkColorSetRGB(0xFF, 0xFF, 0xFF);

constexpr int kVectorIconSize = 20;
constexpr gfx::Insets kVectorIconPadding(14);

// Return FontList for the given absolute font size and font weight.
gfx::FontList GetFontListForSizeAndWeight(int font_size,
                                          gfx::Font::Weight weight);

}  // namespace message_center_style

}  // namespace ash

#endif  // ASH_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
