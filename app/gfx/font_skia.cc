// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/font.h"

#include <gdk/gdk.h>
#include <map>
#include <pango/pango.h>

#include "app/gfx/canvas.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace {

// The font family name which is used when a user's application font for
// GNOME/KDE is a non-scalable one. The name should be listed in the
// IsFallbackFontAllowed function in skia/ext/SkFontHost_fontconfig_direct.cpp.
const char* kFallbackFontFamilyName = "sans";

// Pango scales font sizes. This returns the scale factor. See
// pango_cairo_context_set_resolution for details.
// NOTE: this isn't entirely accurate, in that Pango also consults the
// FC_PIXEL_SIZE first (see get_font_size in pangocairo-fcfont), but this
// seems to give us the same sizes as used by Pango for all our fonts in both
// English and Thia.
static double GetPangoScaleFactor() {
  static float scale_factor = 0;
  static bool determined_scale = false;
  if (!determined_scale) {
    PangoContext* context = gdk_pango_context_get();
    scale_factor = pango_cairo_context_get_resolution(context);
    g_object_unref(context);
    if (scale_factor <= 0)
      scale_factor = 1;
    else
      scale_factor /= 72.0;
  }
  return scale_factor;
}

// Retrieves the pango metrics for a pango font description. Caches the metrics
// and never frees them. The metrics objects are relatively small and
// very expensive to look up.
static PangoFontMetrics* GetPangoFontMetrics(PangoFontDescription* desc) {
  static std::map<int, PangoFontMetrics*>* desc_to_metrics = NULL;
  static PangoContext* context = NULL;

  if (!context) {
    context = gdk_pango_context_get_for_screen(gdk_screen_get_default());
    pango_context_set_language(context, pango_language_get_default());
  }

  if (!desc_to_metrics) {
    desc_to_metrics = new std::map<int, PangoFontMetrics*>();
  }

  int desc_hash = pango_font_description_hash(desc);
  std::map<int, PangoFontMetrics*>::iterator i =
      desc_to_metrics->find(desc_hash);

  if (i == desc_to_metrics->end()) {
    PangoFontMetrics* metrics = pango_context_get_metrics(context, desc, NULL);
    (*desc_to_metrics)[desc_hash] = metrics;
    return metrics;
  } else {
    return i->second;
  }
}

}  // namespace

namespace gfx {

Font::Font(const Font& other) {
  CopyFont(other);
}

Font& Font::operator=(const Font& other) {
  CopyFont(other);
  return *this;
}

Font::Font(SkTypeface* tf, const std::wstring& font_family, int font_size,
           int style)
    : typeface_helper_(new SkAutoUnref(tf)),
      typeface_(tf),
      font_family_(font_family),
      font_size_(font_size),
      style_(style),
      pango_metrics_inited_(false),
      avg_width_(0.0),
      underline_position_(0.0),
      underline_thickness_(0.0) {
  tf->ref();
  calculateMetrics();
}

void Font::calculateMetrics() {
  SkPaint paint;
  SkPaint::FontMetrics metrics;
  PaintSetup(&paint);
  paint.getFontMetrics(&metrics);

  ascent_ = SkScalarCeil(-metrics.fAscent);
  height_ = ascent_ + SkScalarCeil(metrics.fDescent);

}

void Font::CopyFont(const Font& other) {
  typeface_helper_.reset(new SkAutoUnref(other.typeface_));
  typeface_ = other.typeface_;
  typeface_->ref();
  font_family_ = other.font_family_;
  font_size_ = other.font_size_;
  style_ = other.style_;
  height_ = other.height_;
  ascent_ = other.ascent_;
  pango_metrics_inited_ = other.pango_metrics_inited_;
  avg_width_ = other.avg_width_;
  underline_position_ = other.underline_position_;
  underline_thickness_ = other.underline_thickness_;
}

int Font::height() const {
  return height_;
}

int Font::baseline() const {
  return ascent_;
}

int Font::ave_char_width() const {
  return SkScalarRound(avg_width());
}

Font Font::CreateFont(const std::wstring& font_family, int font_size) {
  DCHECK_GT(font_size, 0);
  std::wstring fallback;

  SkTypeface* tf = SkTypeface::CreateFromName(
      base::SysWideToUTF8(font_family).c_str(), SkTypeface::kNormal);
  if (!tf) {
    // A non-scalable font such as .pcf is specified. Falls back to a default
    // scalable font.
    tf = SkTypeface::CreateFromName(
        kFallbackFontFamilyName, SkTypeface::kNormal);
    CHECK(tf) << "Could not find any font: "
              << base::SysWideToUTF8(font_family)
              << ", " << kFallbackFontFamilyName;
    fallback = base::SysUTF8ToWide(kFallbackFontFamilyName);
  }
  SkAutoUnref tf_helper(tf);

  return Font(
      tf, fallback.empty() ? font_family : fallback, font_size, NORMAL);
}

Font Font::DeriveFont(int size_delta, int style) const {
  // If the delta is negative, if must not push the size below 1
  if (size_delta < 0) {
    DCHECK_LT(-size_delta, font_size_);
  }

  if (style == style_) {
    // Fast path, we just use the same typeface at a different size
    return Font(typeface_, font_family_, font_size_ + size_delta, style_);
  }

  // If the style has changed we may need to load a new face
  int skstyle = SkTypeface::kNormal;
  if (BOLD & style)
    skstyle |= SkTypeface::kBold;
  if (ITALIC & style)
    skstyle |= SkTypeface::kItalic;

  SkTypeface* tf = SkTypeface::CreateFromName(
      base::SysWideToUTF8(font_family_).c_str(),
      static_cast<SkTypeface::Style>(skstyle));
  SkAutoUnref tf_helper(tf);

  return Font(tf, font_family_, font_size_ + size_delta, style);
}

void Font::PaintSetup(SkPaint* paint) const {
  paint->setAntiAlias(false);
  paint->setSubpixelText(false);
  paint->setTextSize(SkFloatToScalar(font_size_ * GetPangoScaleFactor()));
  paint->setTypeface(typeface_);
  paint->setFakeBoldText((BOLD & style_) && !typeface_->isBold());
  paint->setTextSkewX((ITALIC & style_) && !typeface_->isItalic() ?
                      -SK_Scalar1/4 : 0);
}

int Font::GetStringWidth(const std::wstring& text) const {
  int width = 0, height = 0;

  Canvas::SizeStringInt(text, *this, &width, &height, 0);
  return width;
}

void Font::InitPangoMetrics() {
  if (!pango_metrics_inited_) {
    pango_metrics_inited_ = true;
    PangoFontDescription* pango_desc = PangoFontFromGfxFont(*this);
    PangoFontMetrics* pango_metrics = GetPangoFontMetrics(pango_desc);

    underline_position_ =
        pango_font_metrics_get_underline_position(pango_metrics);
    underline_position_ /= PANGO_SCALE;

    // todo(davemoore) Come up with a better solution.
    // This is a hack, but without doing this the underlines
    // we get end up fuzzy. So we align to the midpoint of a pixel.
    underline_position_ /= 2;

    underline_thickness_ =
        pango_font_metrics_get_underline_thickness(pango_metrics);
    underline_thickness_ /= PANGO_SCALE;

    // First get the pango based width
    double pango_width =
        pango_font_metrics_get_approximate_char_width(pango_metrics);
    pango_width /= PANGO_SCALE;

    // Yes, this is how Microsoft recommends calculating the dialog unit
    // conversions.
    int text_width = GetStringWidth(
        L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    double dialog_units = (text_width / 26 + 1) / 2;
    avg_width_ = std::min(pango_width, dialog_units);
    pango_font_description_free(pango_desc);
  }
}

double Font::avg_width() const {
  const_cast<Font*>(this)->InitPangoMetrics();
  return avg_width_;
}

double Font::underline_position() const {
  const_cast<Font*>(this)->InitPangoMetrics();
  return underline_position_;
}

double Font::underline_thickness() const {
  const_cast<Font*>(this)->InitPangoMetrics();
  return underline_thickness_;
}

int Font::GetExpectedTextWidth(int length) const {
  double char_width = const_cast<Font*>(this)->avg_width();
  return round(static_cast<float>(length) * char_width);
}

int Font::style() const {
  return style_;
}

std::wstring Font::FontName() {
  return font_family_;
}

int Font::FontSize() {
  return font_size_;
}

NativeFont Font::nativeFont() const {
  return typeface_;
}

}  // namespace gfx
