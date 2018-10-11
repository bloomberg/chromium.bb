// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

namespace message_center_style {

constexpr SkColor kBackgroundColor = SK_ColorWHITE;
constexpr SkColor kEmptyViewColor = SkColorSetARGB(0x8A, 0x0, 0x0, 0x0);
constexpr SkColor kScrollShadowColor = SkColorSetARGB(0x24, 0x0, 0x0, 0x0);

constexpr int kEmptyIconSize = 24;
constexpr gfx::Insets kActionIconPadding(14);
constexpr gfx::Insets kEmptyIconPadding(0, 0, 4, 0);

constexpr int kScrollShadowOffsetY = 2;
constexpr int kScrollShadowBlur = 2;

constexpr int kSettingsTransitionDurationMs = 500;

}  // namespace message_center_style

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_STYLE_H_
