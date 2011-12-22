// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drop_shadow_label.h"

#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"

using views::Label;

namespace chromeos {

static const int kDefaultDropShadowSize = 2;

DropShadowLabel::DropShadowLabel() : drop_shadow_size_(kDefaultDropShadowSize) {
}

void DropShadowLabel::SetDropShadowSize(int drop_shadow_size) {
  if (drop_shadow_size != drop_shadow_size_) {
    drop_shadow_size_ = drop_shadow_size;
    invalidate_text_size();
    SchedulePaint();
  }
}

void DropShadowLabel::PaintText(gfx::Canvas* canvas,
                                const string16& text,
                                const gfx::Rect& text_bounds,
                                int flags) {
  SkColor text_color = enabled() ? enabled_color() : disabled_color();
  if (drop_shadow_size_ > 0) {
    const float kShadowOpacity = 0.2;
    const SkColor shadow_color =
        SkColorSetA(SK_ColorBLACK, kShadowOpacity * SkColorGetA(text_color));
    for (int i = 0; i < drop_shadow_size_; i++) {
      canvas->DrawStringInt(text, font(), shadow_color,
                            text_bounds.x() + i, text_bounds.y(),
                            text_bounds.width(), text_bounds.height(), flags);
      canvas->DrawStringInt(text, font(), shadow_color,
                            text_bounds.x() + i, text_bounds.y() + i,
                            text_bounds.width(), text_bounds.height(), flags);
      canvas->DrawStringInt(text, font(), shadow_color,
                            text_bounds.x(), text_bounds.y() + i,
                            text_bounds.width(), text_bounds.height(), flags);
    }
  }

  canvas->DrawStringInt(text, font(), text_color, text_bounds.x(),
      text_bounds.y(), text_bounds.width(), text_bounds.height(), flags);

  if (HasFocus() || paint_as_focused()) {
    gfx::Rect focus_bounds = text_bounds;
    focus_bounds.Inset(-Label::kFocusBorderPadding,
                       -Label::kFocusBorderPadding);
    canvas->DrawFocusRect(focus_bounds);
  }
}

gfx::Size DropShadowLabel::GetTextSize() const {
  gfx::Size text_size = Label::GetTextSize();
  text_size.SetSize(text_size.width() + drop_shadow_size_,
                    text_size.height() + drop_shadow_size_);
  return text_size;
}

}  // namespace chromeos
