// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/font.h"

#include <fontconfig/fontconfig.h>
#include <gtk/gtk.h>

#include "app/gfx/canvas.h"
#include "base/string_util.h"

namespace gfx {

namespace {

// Returns a PangoContext that is used to get metrics. The returned context
// should never be freed.
PangoContext* get_context() {
  static PangoContext* context = NULL;
  if (!context) {
    context = gdk_pango_context_get_for_screen(gdk_screen_get_default());
    pango_context_set_language(context, gtk_get_default_language());
  }
  return context;
}

}

Font* Font::default_font_ = NULL;

// static
Font Font::CreateFont(PangoFontDescription* desc) {
  return Font(desc);
}

Font Font::DeriveFont(int size_delta, int style) const {
  // If the delta is negative, if must not push the size below 1
  if (size_delta < 0)
    DCHECK_LT(-size_delta, font_ref_->size());

  PangoFontDescription* pfd = pango_font_description_copy(nativeFont());
  pango_font_description_set_size(
      pfd, (font_ref_->size() + size_delta) * PANGO_SCALE);
  pango_font_description_set_style(
      pfd, style & ITALIC ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL);
  pango_font_description_set_weight(
      pfd, style & BOLD ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
  Font font(pfd);
  pango_font_description_free(pfd);
  return font;
}

int Font::height() const {
  return font_ref_->height();
}

int Font::baseline() const {
  return font_ref_->ascent();
}

int Font::ave_char_width() const {
  return font_ref_->ave_char_width();
}

int Font::GetStringWidth(const std::wstring& text) const {
  int width = 0, height = 0;

  Canvas::SizeStringInt(text, *this, &width, &height, 0);
  return width;
}

int Font::GetExpectedTextWidth(int length) const {
  return length * font_ref_->ave_char_width();
}

int Font::style() const {
  return font_ref_->style();
}

std::wstring Font::FontName() {
  return font_ref_->family();
}

int Font::FontSize() {
  return font_ref_->size();
}

PangoFontDescription* Font::nativeFont() const {
  return font_ref_->pfd();
}

// Get the default gtk system font (name and size).
Font::Font() {
  if (default_font_ == NULL) {
    GtkSettings* settings = gtk_settings_get_default();

    gchar* font_name = NULL;
    g_object_get(G_OBJECT(settings),
                 "gtk-font-name", &font_name,
                 NULL);

    // Temporary CHECK for helping track down
    // http://code.google.com/p/chromium/issues/detail?id=12530
    CHECK(font_name) << " Unable to get gtk-font-name for default font.";

    PangoFontDescription* desc =
        pango_font_description_from_string(font_name);
    default_font_ = new Font(desc);
    pango_font_description_free(desc);
    g_free(font_name);
  }

  *this = *default_font_;
}

// static
Font Font::CreateFont(const std::wstring& font_family, int font_size) {
  DCHECK_GT(font_size, 0);

  PangoFontDescription* pfd = pango_font_description_new();
  pango_font_description_set_family(pfd, WideToUTF8(font_family).c_str());
  pango_font_description_set_size(pfd, font_size * PANGO_SCALE);
  pango_font_description_set_style(pfd, PANGO_STYLE_NORMAL);
  pango_font_description_set_weight(pfd, PANGO_WEIGHT_NORMAL);
  Font font(pfd);
  pango_font_description_free(pfd);
  return font;
}

Font::Font(PangoFontDescription* desc) {
  PangoContext* context = get_context();
  pango_context_set_font_description(context, desc);
  PangoFontMetrics* metrics = pango_context_get_metrics(context, desc, NULL);
  int ascent = pango_font_metrics_get_ascent(metrics) / PANGO_SCALE;
  int height = ascent + pango_font_metrics_get_descent(metrics) / PANGO_SCALE;
  int size = pango_font_description_get_size(desc) / PANGO_SCALE;
  int style = 0;
  if (pango_font_description_get_weight(desc) >= PANGO_WEIGHT_BOLD)
    style |= BOLD;
  if (pango_font_description_get_style(desc) == PANGO_STYLE_ITALIC)
    style |= ITALIC;
  // TODO(deanm): How to do underlined?  Where do we use it?  Probably have
  // to paint it ourselves, see pango_font_metrics_get_underline_position.
  int avg_width = pango_font_metrics_get_approximate_char_width(metrics) /
                  PANGO_SCALE;
  std::wstring name(UTF8ToWide(pango_font_description_get_family(desc)));
  font_ref_ = new PangoFontRef(desc, name, size, style, height, ascent,
                               avg_width);
  pango_font_metrics_unref(metrics);
}

Font::PangoFontRef::PangoFontRef(PangoFontDescription* pfd,
                                 const std::wstring& family,
                                 int size,
                                 int style,
                                 int height,
                                 int ascent,
                                 int ave_char_width)
    : pfd_(pango_font_description_copy(pfd)),
      family_(family),
      size_(size),
      style_(style),
      height_(height),
      ascent_(ascent),
      ave_char_width_(ave_char_width) {
}

Font::PangoFontRef::~PangoFontRef() {
  pango_font_description_free(pfd_);
}

}  // namespace gfx
