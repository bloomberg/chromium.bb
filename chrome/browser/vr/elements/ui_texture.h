// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_TEXTURE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/vr/color_scheme.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

class SkCanvas;

namespace gfx {

class FontList;
class PointF;
class RenderText;

}  // namespace gfx

namespace vr {

class UiTexture {
 public:
  UiTexture();
  virtual ~UiTexture();

  void DrawAndLayout(SkCanvas* canvas, const gfx::Size& texture_size);
  virtual gfx::Size GetPreferredTextureSize(int maximum_width) const = 0;
  virtual gfx::SizeF GetDrawnSize() const = 0;
  virtual bool HitTest(const gfx::PointF& point) const;

  bool dirty() const { return dirty_; }

  void SetMode(ColorScheme::Mode mode);

  // This function sets |font_list| to a list of available fonts for |text|. If
  // no font supports |text|, it returns false and leave |font_list| untouched.
  static bool GetFontList(int size,
                          base::string16 text,
                          gfx::FontList* font_list);

 protected:
  enum TextAlignment {
    kTextAlignmentNone,
    kTextAlignmentLeft,
    kTextAlignmentCenter,
    kTextAlignmentRight,
  };

  enum WrappingBehavior {
    kWrappingBehaviorWrap,
    kWrappingBehaviorNoWrap,
  };

  virtual void Draw(SkCanvas* canvas, const gfx::Size& texture_size) = 0;

  virtual void OnSetMode();
  ColorScheme::Mode mode() const { return mode_; }
  const ColorScheme& color_scheme() const;

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
      TextAlignment text_alignment,
      WrappingBehavior wrapping_behavior);

  static std::unique_ptr<gfx::RenderText> CreateRenderText(
      const base::string16& text,
      const gfx::FontList& font_list,
      SkColor color,
      TextAlignment text_alignment);

  void set_dirty() { dirty_ = true; }

  static bool IsRTL();
  static gfx::FontList GetDefaultFontList(int size);
  static void SetForceFontFallbackFailureForTesting(bool force);

 private:
  bool dirty_ = true;
  ColorScheme::Mode mode_ = ColorScheme::kModeNormal;

  DISALLOW_COPY_AND_ASSIGN(UiTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_TEXTURE_H_
