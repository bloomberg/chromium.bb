// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_TEXTURE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
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
  void MeasureSize();
  // TODO(bshe): make this pure virtual.
  virtual void OnMeasureSize();
  virtual gfx::Size GetPreferredTextureSize(int maximum_width) const = 0;
  virtual gfx::SizeF GetDrawnSize() const = 0;
  virtual bool LocalHitTest(const gfx::PointF& point) const;

  bool measured() const { return measured_; }
  bool dirty() const { return dirty_; }

  void OnInitialized();

  // Foreground and background colors are used pervasively in textures, but more
  // element-specific colors should be set on the appropriate class.
  void SetForegroundColor(SkColor color);
  void SetBackgroundColor(SkColor color);

  // This function sets |font_list| to a list of available fonts for |text|. If
  // no font supports |text|, it returns false and leave |font_list| untouched.
  static bool GetDefaultFontList(int font_size,
                                 base::string16 text,
                                 gfx::FontList* font_list);

  // This function sets |font_list| to a list of available fonts for |text|. If
  // the font with |preferred_font_name| is available and supports |text|,
  // |font_list| will be configured to use the preferred font instead of default
  // font. If no font supports |text|, it returns false and leave |font_list|
  // untouched.
  static bool GetFontList(const std::string& preferred_font_name,
                          int font_size,
                          base::string16 text,
                          gfx::FontList* font_list);

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

  struct TextRenderParameters {
    SkColor color = SK_ColorBLACK;
    TextAlignment text_alignment = kTextAlignmentNone;
    WrappingBehavior wrapping_behavior = kWrappingBehaviorNoWrap;
    bool cursor_enabled = false;
    int cursor_position = 0;
    bool shadows_enabled = false;
    SkColor shadow_color = SK_ColorBLACK;
    float shadow_size = 10.0f;
  };

 protected:
  virtual void Draw(SkCanvas* canvas, const gfx::Size& texture_size) = 0;

  template <typename T>
  void SetAndDirty(T* target, const T& value) {
    if (*target != value)
      set_dirty();
    *target = value;
  }

  // Prepares a set of RenderText objects with the given parameters.
  // Attempts to fit the text within the provided size. |flags| specifies how
  // the text should be rendered. If multiline is requested and provided height
  // is 0, it will be set to the minimum needed to fit the whole text. If
  // multiline is not requested and provided width is 0, it will be set to the
  // minimum needed to fit the whole text.
  static std::vector<std::unique_ptr<gfx::RenderText>> PrepareDrawStringRect(
      const base::string16& text,
      const gfx::FontList& font_list,
      gfx::Rect* bounds,
      const TextRenderParameters& parameters);

  // Deprecated legacy text prep function. UI elements that use this routine
  // should migrate to use Text elements, rather than drawing text directly.
  static std::vector<std::unique_ptr<gfx::RenderText>> PrepareDrawStringRect(
      const base::string16& text,
      const gfx::FontList& font_list,
      SkColor color,
      gfx::Rect* bounds,
      TextAlignment text_alignment,
      WrappingBehavior wrapping_behavior);

  static std::unique_ptr<gfx::RenderText> CreateRenderText();

  static std::unique_ptr<gfx::RenderText> CreateConfiguredRenderText(
      const base::string16& text,
      const gfx::FontList& font_list,
      SkColor color,
      TextAlignment text_alignment,
      bool shadows_enabled,
      SkColor shadow_color,
      float shadow_size);

  static bool IsRTL();
  static void SetForceFontFallbackFailureForTesting(bool force);

  void set_dirty() {
    measured_ = false;
    dirty_ = true;
  }

  SkColor foreground_color() const;
  SkColor background_color() const;

 private:
  bool dirty_ = true;
  bool measured_ = false;
  base::Optional<SkColor> foreground_color_;
  base::Optional<SkColor> background_color_;

  DISALLOW_COPY_AND_ASSIGN(UiTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_TEXTURE_H_
