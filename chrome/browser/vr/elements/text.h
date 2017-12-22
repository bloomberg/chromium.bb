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

enum TextLayoutMode {
  kSingleLineFixedWidth,
  kSingleLineFixedHeight,
  kMultiLineFixedWidth,
};

class Text : public TexturedElement {
 public:
  explicit Text(float font_height_dmms);
  ~Text() override;

  void SetFontHeightInDmm(float font_height_dmms);
  void SetText(const base::string16& text);
  void SetColor(SkColor color);

  void SetTextAlignment(UiTexture::TextAlignment alignment);
  void SetTextLayoutMode(TextLayoutMode mode);

  // This text element does not typically feature a cursor, but since the cursor
  // position is deterined while laying out text, a parent may wish to supply
  // cursor parameters and determine where the cursor was last drawn.
  void SetCursorEnabled(bool enabled);
  void SetCursorPosition(int position);

  // Returns the most recently computed cursor position, in pixels.  This is
  // used for scene dirtiness and testing.
  gfx::Rect GetRawCursorBounds() const;

  // Returns the most recently computed cursor position, in fractions of the
  // texture size, relative to the upper-left corner of the element.
  gfx::RectF GetCursorBounds() const;

  void OnSetSize(const gfx::SizeF& size) override;
  void UpdateElementSize() override;

  const std::vector<std::unique_ptr<gfx::RenderText>>& LayOutTextForTest();
  gfx::SizeF GetTextureSizeForTest() const;

 private:
  UiTexture* GetTexture() const override;

  TextLayoutMode text_layout_mode_ = kMultiLineFixedWidth;
  std::unique_ptr<TextTexture> texture_;
  DISALLOW_COPY_AND_ASSIGN(Text);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TEXT_H_
