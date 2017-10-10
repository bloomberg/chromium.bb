// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/message_center/message_center_style.h"

namespace ash {

namespace message_center_style {

gfx::FontList GetFontListForSizeAndWeight(int font_size,
                                          gfx::Font::Weight weight) {
  gfx::Font default_font;
  int font_size_delta = font_size - default_font.GetFontSize();
  gfx::Font font =
      default_font.Derive(font_size_delta, gfx::Font::NORMAL, weight);
  DCHECK_EQ(font_size, font.GetFontSize());
  return gfx::FontList(font);
}

}  // namespace message_center_style

}  // namespace ash
