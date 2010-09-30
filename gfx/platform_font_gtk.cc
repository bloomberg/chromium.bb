// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/platform_font_gtk.h"

#include <algorithm>
#include <fontconfig/fontconfig.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <map>
#include <pango/pango.h>

#include "base/logging.h"
#include "base/string_piece.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "gfx/canvas_skia.h"
#include "gfx/font.h"
#include "gfx/gtk_util.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace {

// The font family name which is used when a user's application font for
// GNOME/KDE is a non-scalable one. The name should be listed in the
// IsFallbackFontAllowed function in skia/ext/SkFontHost_fontconfig_direct.cpp.
const char* kFallbackFontFamilyName = "sans";

// Retrieves the pango metrics for a pango font description. Caches the metrics
// and never frees them. The metrics objects are relatively small and
// very expensive to look up.
PangoFontMetrics* GetPangoFontMetrics(PangoFontDescription* desc) {
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

// Find the best match font for |family_name| in the same way as Skia
// to make sure CreateFont() successfully creates a default font.  In
// Skia, it only checks the best match font.  If it failed to find
// one, SkTypeface will be NULL for that font family.  It eventually
// causes a segfault.  For example, family_name = "Sans" and system
// may have various fonts.  The first font family in FcPattern will be
// "DejaVu Sans" but a font family returned by FcFontMatch will be "VL
// PGothic".  In this case, SkTypeface for "Sans" returns NULL even if
// the system has a font for "Sans" font family.  See FontMatch() in
// skia/ports/SkFontHost_fontconfig.cpp for more detail.
std::wstring FindBestMatchFontFamilyName(const char* family_name) {
  FcPattern* pattern = FcPatternCreate();
  FcValue fcvalue;
  fcvalue.type = FcTypeString;
  char* family_name_copy = strdup(family_name);
  fcvalue.u.s = reinterpret_cast<FcChar8*>(family_name_copy);
  FcPatternAdd(pattern, FC_FAMILY, fcvalue, 0);
  FcConfigSubstitute(0, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);
  FcResult result;
  FcPattern* match = FcFontMatch(0, pattern, &result);
  DCHECK(match) << "Could not find font: " << family_name;
  FcChar8* match_family;
  FcPatternGetString(match, FC_FAMILY, 0, &match_family);

  std::wstring font_family = UTF8ToWide(reinterpret_cast<char*>(match_family));
  FcPatternDestroy(match);
  FcPatternDestroy(pattern);
  free(family_name_copy);
  return font_family;
}

}  // namespace

namespace gfx {

Font* PlatformFontGtk::default_font_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// PlatformFontGtk, public:

PlatformFontGtk::PlatformFontGtk() {
  if (default_font_ == NULL) {
    GtkSettings* settings = gtk_settings_get_default();

    gchar* font_name = NULL;
    g_object_get(settings, "gtk-font-name", &font_name, NULL);

    // Temporary CHECK for helping track down
    // http://code.google.com/p/chromium/issues/detail?id=12530
    CHECK(font_name) << " Unable to get gtk-font-name for default font.";

    PangoFontDescription* desc =
        pango_font_description_from_string(font_name);
    default_font_ = new Font(desc);
    pango_font_description_free(desc);
    g_free(font_name);

    DCHECK(default_font_);
  }

  InitFromPlatformFont(
      static_cast<PlatformFontGtk*>(default_font_->platform_font()));
}

PlatformFontGtk::PlatformFontGtk(const Font& other) {
  InitFromPlatformFont(
      static_cast<PlatformFontGtk*>(other.platform_font()));
}

PlatformFontGtk::PlatformFontGtk(NativeFont native_font) {
  gint size = pango_font_description_get_size(native_font);
  const char* family_name = pango_font_description_get_family(native_font);

  if (pango_font_description_get_size_is_absolute(native_font)) {
    // font_size_ is treated as scaled (see comment in GetPangoScaleFactor). If
    // we get here the font size is absolute though, and we need to invert the
    // scale so that when scaled in the rest of this class everything lines up.
    size /= PlatformFontGtk::GetPangoScaleFactor();
  }

  // Find best match font for |family_name| to make sure we can get
  // a SkTypeface for the default font.
  // TODO(agl): remove this.
  std::wstring font_family = FindBestMatchFontFamilyName(family_name);

  InitWithNameAndSize(font_family, size / PANGO_SCALE);
  int style = 0;
  if (pango_font_description_get_weight(native_font) == PANGO_WEIGHT_BOLD) {
    // TODO(davemoore) What should we do about other weights? We currently
    // only support BOLD.
    style |= gfx::Font::BOLD;
  }
  if (pango_font_description_get_style(native_font) == PANGO_STYLE_ITALIC) {
    // TODO(davemoore) What about PANGO_STYLE_OBLIQUE?
    style |= gfx::Font::ITALIC;
  }
  if (style != 0)
    style_ = style;
}

PlatformFontGtk::PlatformFontGtk(const std::wstring& font_name,
                                 int font_size) {
  InitWithNameAndSize(font_name, font_size);
}

double PlatformFontGtk::underline_position() const {
  const_cast<PlatformFontGtk*>(this)->InitPangoMetrics();
  return underline_position_;
}

double PlatformFontGtk::underline_thickness() const {
  const_cast<PlatformFontGtk*>(this)->InitPangoMetrics();
  return underline_thickness_;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontGtk, PlatformFont implementation:

Font PlatformFontGtk::DeriveFont(int size_delta, int style) const {
  // If the delta is negative, if must not push the size below 1
  if (size_delta < 0)
    DCHECK_LT(-size_delta, font_size_);

  if (style == style_) {
    // Fast path, we just use the same typeface at a different size
    return Font(new PlatformFontGtk(typeface_,
                                    font_family_,
                                    font_size_ + size_delta,
                                    style_));
  }

  // If the style has changed we may need to load a new face
  int skstyle = SkTypeface::kNormal;
  if (gfx::Font::BOLD & style)
    skstyle |= SkTypeface::kBold;
  if (gfx::Font::ITALIC & style)
    skstyle |= SkTypeface::kItalic;

  SkTypeface* typeface = SkTypeface::CreateFromName(
      base::SysWideToUTF8(font_family_).c_str(),
      static_cast<SkTypeface::Style>(skstyle));
  SkAutoUnref tf_helper(typeface);

  return Font(new PlatformFontGtk(typeface,
                                  font_family_,
                                  font_size_ + size_delta,
                                  style));
}

int PlatformFontGtk::GetHeight() const {
  return height_;
}

int PlatformFontGtk::GetBaseline() const {
  return ascent_;
}

int PlatformFontGtk::GetAverageCharacterWidth() const {
  return SkScalarRound(average_width_);
}

int PlatformFontGtk::GetStringWidth(const std::wstring& text) const {
  int width = 0, height = 0;
  CanvasSkia::SizeStringInt(text, Font(const_cast<PlatformFontGtk*>(this)),
                            &width, &height, gfx::Canvas::NO_ELLIPSIS);
  return width;
}

int PlatformFontGtk::GetExpectedTextWidth(int length) const {
  double char_width = const_cast<PlatformFontGtk*>(this)->GetAverageWidth();
  return round(static_cast<float>(length) * char_width);
}

int PlatformFontGtk::GetStyle() const {
  return style_;
}

const std::wstring& PlatformFontGtk::GetFontName() const {
  return font_family_;
}

int PlatformFontGtk::GetFontSize() const {
  return font_size_;
}

NativeFont PlatformFontGtk::GetNativeFont() const {
  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, WideToUTF8(GetFontName()).c_str());
  // Set the absolute size to avoid overflowing UI elements.
  pango_font_description_set_absolute_size(pfd,
      GetFontSize() * PANGO_SCALE * GetPangoScaleFactor());

  switch (GetStyle()) {
    case gfx::Font::NORMAL:
      // Nothing to do, should already be PANGO_STYLE_NORMAL.
      break;
    case gfx::Font::BOLD:
      pango_font_description_set_weight(pfd, PANGO_WEIGHT_BOLD);
      break;
    case gfx::Font::ITALIC:
      pango_font_description_set_style(pfd, PANGO_STYLE_ITALIC);
      break;
    case gfx::Font::UNDERLINED:
      // TODO(deanm): How to do underlined?  Where do we use it?  Probably have
      // to paint it ourselves, see pango_font_metrics_get_underline_position.
      break;
  }

  return pfd;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFontGtk, private:

PlatformFontGtk::PlatformFontGtk(SkTypeface* typeface,
                                 const std::wstring& name,
                                 int size,
                                 int style) {
  InitWithTypefaceNameSizeAndStyle(typeface, name, size, style);
}

PlatformFontGtk::~PlatformFontGtk() {}

void PlatformFontGtk::InitWithNameAndSize(const std::wstring& font_name,
                                          int font_size) {
  DCHECK_GT(font_size, 0);
  std::wstring fallback;

  SkTypeface* typeface = SkTypeface::CreateFromName(
      base::SysWideToUTF8(font_name).c_str(), SkTypeface::kNormal);
  if (!typeface) {
    // A non-scalable font such as .pcf is specified. Falls back to a default
    // scalable font.
    typeface = SkTypeface::CreateFromName(
        kFallbackFontFamilyName, SkTypeface::kNormal);
    CHECK(typeface) << "Could not find any font: "
                    << base::SysWideToUTF8(font_name)
                    << ", " << kFallbackFontFamilyName;
    fallback = base::SysUTF8ToWide(kFallbackFontFamilyName);
  }
  SkAutoUnref typeface_helper(typeface);

  InitWithTypefaceNameSizeAndStyle(typeface,
                                   fallback.empty() ? font_name : fallback,
                                   font_size,
                                   gfx::Font::NORMAL);
}

void PlatformFontGtk::InitWithTypefaceNameSizeAndStyle(
    SkTypeface* typeface,
    const std::wstring& font_family,
    int font_size,
    int style) {
  typeface_helper_.reset(new SkAutoUnref(typeface));
  typeface_ = typeface;
  typeface_->ref();
  font_family_ = font_family;
  font_size_ = font_size;
  style_ = style;
  pango_metrics_inited_ = false;
  average_width_ = 0.0f;
  underline_position_ = 0.0f;
  underline_thickness_ = 0.0f;

  SkPaint paint;
  SkPaint::FontMetrics metrics;
  PaintSetup(&paint);
  paint.getFontMetrics(&metrics);

  ascent_ = SkScalarCeil(-metrics.fAscent);
  height_ = ascent_ + SkScalarCeil(metrics.fDescent);
}

void PlatformFontGtk::InitFromPlatformFont(const PlatformFontGtk* other) {
  typeface_helper_.reset(new SkAutoUnref(other->typeface_));
  typeface_ = other->typeface_;
  typeface_->ref();
  font_family_ = other->font_family_;
  font_size_ = other->font_size_;
  style_ = other->style_;
  height_ = other->height_;
  ascent_ = other->ascent_;
  pango_metrics_inited_ = other->pango_metrics_inited_;
  average_width_ = other->average_width_;
  underline_position_ = other->underline_position_;
  underline_thickness_ = other->underline_thickness_;
}

void PlatformFontGtk::PaintSetup(SkPaint* paint) const {
  paint->setAntiAlias(false);
  paint->setSubpixelText(false);
  paint->setTextSize(
      SkFloatToScalar(font_size_ * PlatformFontGtk::GetPangoScaleFactor()));
  paint->setTypeface(typeface_);
  paint->setFakeBoldText((gfx::Font::BOLD & style_) && !typeface_->isBold());
  paint->setTextSkewX((gfx::Font::ITALIC & style_) && !typeface_->isItalic() ?
                      -SK_Scalar1/4 : 0);
}

void PlatformFontGtk::InitPangoMetrics() {
  if (!pango_metrics_inited_) {
    pango_metrics_inited_ = true;
    PangoFontDescription* pango_desc = GetNativeFont();
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
    average_width_ = std::min(pango_width, dialog_units);
    pango_font_description_free(pango_desc);
  }
}


float PlatformFontGtk::GetPangoScaleFactor() {
  // Pango scales font sizes. This returns the scale factor. See
  // pango_cairo_context_set_resolution for details.
  // NOTE: this isn't entirely accurate, in that Pango also consults the
  // FC_PIXEL_SIZE first (see get_font_size in pangocairo-fcfont), but this
  // seems to give us the same sizes as used by Pango for all our fonts in both
  // English and Thai.
  static float scale_factor = gfx::GetPangoResolution();
  static bool determined_scale = false;
  if (!determined_scale) {
    if (scale_factor <= 0)
      scale_factor = 1;
    else
      scale_factor /= 72.0;
    determined_scale = true;
  }
  return scale_factor;
}

double PlatformFontGtk::GetAverageWidth() const {
  const_cast<PlatformFontGtk*>(this)->InitPangoMetrics();
  return average_width_;
}

////////////////////////////////////////////////////////////////////////////////
// PlatformFont, public:

// static
PlatformFont* PlatformFont::CreateDefault() {
  return new PlatformFontGtk;
}

// static
PlatformFont* PlatformFont::CreateFromFont(const Font& other) {
  return new PlatformFontGtk(other);
}

// static
PlatformFont* PlatformFont::CreateFromNativeFont(NativeFont native_font) {
  return new PlatformFontGtk(native_font);
}

// static
PlatformFont* PlatformFont::CreateFromNameAndSize(const std::wstring& font_name,
                                                  int font_size) {
  return new PlatformFontGtk(font_name, font_size);
}

}  // namespace gfx
