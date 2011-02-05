// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drop_shadow_label.h"

#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"

using views::Label;

namespace chromeos {

static const int kDefaultDropShadowSize = 2;

// Default color is black.
static const SkColor kDefaultColor = 0x000000;

static const float kShadowOpacity = 0.2;

DropShadowLabel::DropShadowLabel() : Label() {
  Init();
}

void DropShadowLabel::Init() {
  drop_shadow_size_ = kDefaultDropShadowSize;
}

void DropShadowLabel::SetDropShadowSize(int drop_shadow_size) {
  if (drop_shadow_size != drop_shadow_size_) {
    drop_shadow_size_ = drop_shadow_size;
    invalidate_text_size();
    SchedulePaint();
  }
}

void DropShadowLabel::PaintText(gfx::Canvas* canvas,
                                const std::wstring& text,
                                const gfx::Rect& text_bounds,
                                int flags) {
  if (drop_shadow_size_ > 0) {
    SkColor color = SkColorSetARGB(kShadowOpacity * SkColorGetA(GetColor()),
                                   SkColorGetR(kDefaultColor),
                                   SkColorGetG(kDefaultColor),
                                   SkColorGetB(kDefaultColor));
    for (int i = 0; i < drop_shadow_size_; i++) {
      canvas->DrawStringInt(WideToUTF16Hack(text), font(), color,
                            text_bounds.x() + i, text_bounds.y(),
                            text_bounds.width(), text_bounds.height(), flags);
      canvas->DrawStringInt(WideToUTF16Hack(text), font(), color,
                            text_bounds.x() + i, text_bounds.y() + i,
                            text_bounds.width(), text_bounds.height(), flags);
      canvas->DrawStringInt(WideToUTF16Hack(text), font(), color,
                            text_bounds.x(), text_bounds.y() + i,
                            text_bounds.width(), text_bounds.height(), flags);
    }
  }

  canvas->DrawStringInt(WideToUTF16Hack(text), font(), GetColor(),
                        text_bounds.x(), text_bounds.y(),
                        text_bounds.width(), text_bounds.height(), flags);

  if (HasFocus() || paint_as_focused()) {
    gfx::Rect focus_bounds = text_bounds;
    focus_bounds.Inset(-Label::kFocusBorderPadding,
                       -Label::kFocusBorderPadding);
    canvas->DrawFocusRect(focus_bounds.x(), focus_bounds.y(),
                          focus_bounds.width(), focus_bounds.height());
  }
}

gfx::Size DropShadowLabel::GetTextSize() const {
  gfx::Size text_size = Label::GetTextSize();
  text_size.SetSize(text_size.width() + drop_shadow_size_,
                    text_size.height() + drop_shadow_size_);
  return text_size;
}

}  // namespace chromeos
