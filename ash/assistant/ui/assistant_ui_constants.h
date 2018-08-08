// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_UI_CONSTANTS_H_
#define ASH_ASSISTANT_UI_ASSISTANT_UI_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace gfx {
class FontList;
}  // namespace gfx

namespace ash {

// Appearance.
constexpr int kMaxHeightDip = 640;
constexpr int kPaddingDip = 14;
constexpr int kPreferredWidthDip = 640;
constexpr int kSpacingDip = 8;

// Typography.
constexpr SkColor kTextColorHint = gfx::kGoogleGrey500;
constexpr SkColor kTextColorPrimary = gfx::kGoogleGrey900;
constexpr SkColor kTextColorSecondary = gfx::kGoogleGrey700;

// TODO(dmblack): Move the other constants into ash::assistant::ui.
namespace assistant {
namespace ui {

// TODO(dmblack): Remove when enabling. This is used to gate aspects of the
// motion spec which are being developed in pieces which should be enabled
// together.
constexpr bool kIsMotionSpecEnabled = true;

// Returns the default font list for Assistant UI.
const gfx::FontList& GetDefaultFontList();

}  // namespace ui
}  // namespace assistant

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_UI_CONSTANTS_H_
