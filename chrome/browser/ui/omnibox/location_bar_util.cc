// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/location_bar_util.h"

#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/rect.h"

namespace location_bar_util {

string16 CalculateMinString(const string16& description) {
  // Chop at the first '.' or whitespace.
  const size_t dot_index = description.find('.');
  const size_t ws_index = description.find_first_of(kWhitespaceUTF16);
  size_t chop_index = std::min(dot_index, ws_index);
  string16 min_string;
  if (chop_index == string16::npos) {
    // No dot or whitespace, truncate to at most 3 chars.
    min_string = ui::TruncateString(description, 3);
  } else {
    min_string = description.substr(0, chop_index);
  }
  base::i18n::AdjustStringForLocaleDirection(&min_string);
  return min_string;
}


void PaintExtensionActionBackground(const ExtensionAction& action,
                                    int tab_id,
                                    gfx::Canvas* canvas,
                                    const gfx::Rect& bounds,
                                    SkColor text_color,
                                    SkColor background_color) {
  if (!action.WantsAttention(tab_id))
    return;

  SkPoint gradient_bounds[2] = { {SkIntToScalar(bounds.x()),
                                  SkIntToScalar(bounds.y())},
                                 {SkIntToScalar(bounds.x()),
                                  SkIntToScalar(bounds.bottom())} };
  SkColor gradient_colors[2] = {
    color_utils::AlphaBlend(text_color, background_color, 0x13),
    color_utils::AlphaBlend(text_color, background_color, 0x1d)
  };
  skia::RefPtr<SkShader> gradient = skia::AdoptRef(
      SkGradientShader::CreateLinear(gradient_bounds, gradient_colors,
                                     NULL, 2, SkShader::kClamp_TileMode));
  SkPaint paint;
  paint.setShader(gradient.get());
  canvas->DrawRect(bounds, paint);

  SkColor border_color =
      color_utils::AlphaBlend(text_color, background_color, 0x55);
  canvas->DrawLine(bounds.origin(),
                   gfx::Point(bounds.x(), bounds.bottom()),
                   border_color);
  // "-1" because gfx::Rects are half-open, not including their right or
  // bottom edges.
  canvas->DrawLine(gfx::Point(bounds.right() - 1, bounds.y()),
                   gfx::Point(bounds.right() - 1, bounds.bottom()),
                   border_color);
}

}  // namespace location_bar_util
