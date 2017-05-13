// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_UI_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_UI_TEXTURE_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

class SkCanvas;

namespace gfx {

class FontList;
class RenderText;

}  // namespace gfx

namespace vr_shell {

class UiTexture {
 public:
  enum {
    // Flags to configure PrepareDrawStringRect
    TEXT_ALIGN_LEFT = 1 << 0,
    TEXT_ALIGN_CENTER = 1 << 1,
    TEXT_ALIGN_RIGHT = 1 << 2,
    MULTI_LINE = 1 << 3
  };

  UiTexture();
  virtual ~UiTexture();

  void DrawAndLayout(SkCanvas* canvas, const gfx::Size& texture_size);
  virtual gfx::Size GetPreferredTextureSize(int maximum_width) const = 0;
  virtual gfx::SizeF GetDrawnSize() const = 0;
  // Returns true if the state changed.
  virtual bool SetDrawFlags(int draw_flags);
  int GetDrawFlags() { return draw_flags_; }

 protected:
  virtual void Draw(SkCanvas* canvas, const gfx::Size& texture_size) = 0;

  // Prepares a set of RenderText objects with the given color and fonts.
  // Attempts to fit the text within the provided size. |flags| specifies how
  // the text should be rendered. If multiline is requested and provided height
  // is 0, it will be set to the minimum needed to fit the whole text. If
  // multiline is not requested and provided width is 0, it will be set to the
  // minimum needed to fit the whole text.
  static std::vector<std::unique_ptr<gfx::RenderText>> PrepareDrawStringRect(
      const base::string16& text,
      const gfx::FontList& font_list,
      SkColor color,
      gfx::Rect* bounds,
      int flags);

  static bool IsRTL();
  static gfx::FontList GetDefaultFontList(int size);
  static gfx::FontList GetFontList(int size, base::string16 text);

 private:
  int draw_flags_ = 0;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_UI_TEXTURE_H_
