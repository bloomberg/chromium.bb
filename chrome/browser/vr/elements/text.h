// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TEXT_H_

#include <memory>

#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRect.h"

namespace gfx {
class RenderText;
}

namespace vr {

class TextTexture;

class Text : public TexturedElement {
 public:
  Text(int maximum_width_pixels, float font_height_meters);
  ~Text() override;

  void SetText(const base::string16& text);
  void SetColor(SkColor color);

  void SetTextAlignment(UiTexture::TextAlignment alignment);
  void SetMultiLine(bool multiline);

  // This text element does not typically feature a cursor, but since the cursor
  // position is deterined while laying out text, a parent may wish to supply
  // cursor parameters and determine where the cursor was last drawn.
  void SetCursorEnabled(bool enabled);
  void SetCursorPosition(int position);

  // Returns the most recently computed cursor position, in DMM, relative to the
  // corner of the element.
  gfx::RectF GetCursorBounds();

  void OnSetSize(gfx::SizeF size) override;

  std::vector<std::unique_ptr<gfx::RenderText>> LayOutTextForTest(
      const gfx::Size& texture_size);
  gfx::SizeF GetTextureSizeForTest() const;

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<TextTexture> texture_;
  DISALLOW_COPY_AND_ASSIGN(Text);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
