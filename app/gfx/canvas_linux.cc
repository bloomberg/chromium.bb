// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/canvas.h"

#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <pango/pangocairo.h>

#include "app/gfx/font.h"
#include "base/gfx/rect.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace {

// Font settings that we initialize once and then use when drawing text in
// DrawStringInt().
static cairo_font_options_t* cairo_font_options = NULL;

// Returns the resolution used by pango. A negative values means the resolution
// hasn't been set.
static double GetPangoResolution() {
  static double resolution;
  static bool determined_resolution = false;
  if (!determined_resolution) {
    determined_resolution = true;
    PangoContext* default_context = gdk_pango_context_get();
    resolution = pango_cairo_context_get_resolution(default_context);
    g_object_unref(default_context);
  }
  return resolution;
}

// Update |cairo_font_options| based on GtkSettings, allocating it if needed.
static void UpdateCairoFontOptions() {
  if (!cairo_font_options)
    cairo_font_options = cairo_font_options_create();

  GtkSettings* gtk_settings = gtk_settings_get_default();
  gint antialias = 0;
  gint hinting = 0;
  gchar* hint_style = NULL;
  gchar* rgba_style = NULL;
  g_object_get(gtk_settings,
               "gtk-xft-antialias", &antialias,
               "gtk-xft-hinting", &hinting,
               "gtk-xft-hintstyle", &hint_style,
               "gtk-xft-rgba", &rgba_style,
               NULL);

  // g_object_get() doesn't tell us whether the properties were present or not,
  // but if they aren't (because gnome-settings-daemon isn't running), we'll get
  // NULL values for the strings.
  if (hint_style && rgba_style) {
    if (!antialias) {
      cairo_font_options_set_antialias(cairo_font_options,
                                       CAIRO_ANTIALIAS_NONE);
    } else if (strcmp(rgba_style, "none") == 0) {
      cairo_font_options_set_antialias(cairo_font_options,
                                       CAIRO_ANTIALIAS_GRAY);
    } else {
      cairo_font_options_set_antialias(cairo_font_options,
                                       CAIRO_ANTIALIAS_SUBPIXEL);
      cairo_subpixel_order_t cairo_subpixel_order =
          CAIRO_SUBPIXEL_ORDER_DEFAULT;
      if (strcmp(rgba_style, "rgb") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_RGB;
      } else if (strcmp(rgba_style, "bgr") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_BGR;
      } else if (strcmp(rgba_style, "vrgb") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VRGB;
      } else if (strcmp(rgba_style, "vbgr") == 0) {
        cairo_subpixel_order = CAIRO_SUBPIXEL_ORDER_VBGR;
      }
      cairo_font_options_set_subpixel_order(cairo_font_options,
                                            cairo_subpixel_order);
    }

    cairo_hint_style_t cairo_hint_style = CAIRO_HINT_STYLE_DEFAULT;
    if (hinting == 0 || strcmp(hint_style, "hintnone") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_NONE;
    } else if (strcmp(hint_style, "hintslight") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_SLIGHT;
    } else if (strcmp(hint_style, "hintmedium") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_MEDIUM;
    } else if (strcmp(hint_style, "hintfull") == 0) {
      cairo_hint_style = CAIRO_HINT_STYLE_FULL;
    }
    cairo_font_options_set_hint_style(cairo_font_options, cairo_hint_style);
  }

  if (hint_style)
    g_free(hint_style);
  if (rgba_style)
    g_free(rgba_style);
}

}  // namespace

namespace gfx {

Canvas::Canvas(int width, int height, bool is_opaque)
    : skia::PlatformCanvas(width, height, is_opaque) {
}

Canvas::Canvas() : skia::PlatformCanvas() {
}

Canvas::~Canvas() {
}

// Pass a width > 0 to force wrapping and elliding.
static void SetupPangoLayout(PangoLayout* layout,
                             const gfx::Font& font,
                             int width,
                             int flags) {
  if (!cairo_font_options)
    UpdateCairoFontOptions();
  // This needs to be done early on; it has no effect when called just before
  // pango_cairo_show_layout().
  pango_cairo_context_set_font_options(
      pango_layout_get_context(layout), cairo_font_options);

  // Callers of DrawStringInt handle RTL layout themselves, so tell pango to not
  // scope out RTL characters.
  pango_layout_set_auto_dir(layout, FALSE);

  if (width > 0)
    pango_layout_set_width(layout, width * PANGO_SCALE);

  if (flags & Canvas::NO_ELLIPSIS) {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_NONE);
  } else {
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
  }

  if (flags & Canvas::TEXT_ALIGN_CENTER) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
  } else if (flags & Canvas::TEXT_ALIGN_RIGHT) {
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
  }

  if (flags & Canvas::MULTI_LINE) {
    pango_layout_set_wrap(layout,
        (flags & Canvas::CHARACTER_BREAK) ?
            PANGO_WRAP_WORD_CHAR : PANGO_WRAP_WORD);
  }

  // Set the resolution to match that used by Gtk. If we don't set the
  // resolution and the resolution differs from the default, Gtk and Chrome end
  // up drawing at different sizes.
  double resolution = GetPangoResolution();
  if (resolution > 0) {
    pango_cairo_context_set_resolution(pango_layout_get_context(layout),
                                       resolution);
  }

  PangoFontDescription* desc = gfx::Font::PangoFontFromGfxFont(font);
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);
}

// static
void Canvas::SizeStringInt(const std::wstring& text,
                           const gfx::Font& font,
                           int* width, int* height, int flags) {
  int org_width = *width;
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0);
  cairo_t* cr = cairo_create(surface);
  PangoLayout* layout = pango_cairo_create_layout(cr);

  SetupPangoLayout(layout, font, *width, flags);

  std::string utf8 = WideToUTF8(text);
  pango_layout_set_text(layout, utf8.data(), utf8.size());

  pango_layout_get_pixel_size(layout, width, height);

  if (org_width > 0 && flags & Canvas::MULTI_LINE &&
      pango_layout_is_wrapped(layout)) {
    // The text wrapped. There seems to be a bug in Pango when this happens
    // such that the width returned from pango_layout_get_pixel_size is too
    // small. Using the width from pango_layout_get_pixel_size in this case
    // results in wrapping across more lines, which requires a bigger height.
    // As a workaround we use the original width, which is not necessarily
    // exactly correct, but isn't wrong by much.
    //
    // It looks like Pango uses the size of whitespace in calculating wrapping
    // but doesn't include the size of the whitespace when the extents are
    // asked for. See the loop in pango-layout.c process_item that determines
    // where to wrap.
    *width = org_width;
  }

  g_object_unref(layout);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

void Canvas::DrawStringInt(const std::wstring& text,
                           const gfx::Font& font,
                           const SkColor& color,
                           int x, int y, int w, int h,
                           int flags) {
  cairo_t* cr = beginPlatformPaint();
  PangoLayout* layout = pango_cairo_create_layout(cr);

  SetupPangoLayout(layout, font, w, flags);

  pango_layout_set_height(layout, h * PANGO_SCALE);

  cairo_save(cr);
  cairo_set_source_rgb(cr,
                       SkColorGetR(color) / 255.0,
                       SkColorGetG(color) / 255.0,
                       SkColorGetB(color) / 255.0);

  std::string utf8 = WideToUTF8(text);
  pango_layout_set_text(layout, utf8.data(), utf8.size());

  int width, height;
  pango_layout_get_pixel_size(layout, &width, &height);

  cairo_rectangle(cr, x, y, w, h);
  cairo_clip(cr);

  if (flags & Canvas::TEXT_VALIGN_TOP) {
    // Cairo should draw from the top left corner already.
  } else if (flags & Canvas::TEXT_VALIGN_BOTTOM) {
    y += (h - height);
  } else {
    // Vertically centered.
    y += ((h - height) / 2);
  }

  cairo_move_to(cr, x, y);
  pango_cairo_show_layout(cr, layout);
  if (font.style() & gfx::Font::UNDERLINED) {
    double underline_y =
        static_cast<double>(y) + height + font.underline_position();
    cairo_set_line_width(cr, font.underline_thickness());
    cairo_move_to(cr, x, underline_y);
    cairo_line_to(cr, x + width, underline_y);
    cairo_stroke(cr);
  }
  cairo_restore(cr);

  g_object_unref(layout);
  // NOTE: beginPlatformPaint returned its surface, we shouldn't destroy it.
}

void Canvas::DrawGdkPixbuf(GdkPixbuf* pixbuf, int x, int y) {
  if (!pixbuf) {
    NOTREACHED();
    return;
  }

  cairo_t* cr = beginPlatformPaint();
  gdk_cairo_set_source_pixbuf(cr, pixbuf, x, y);
  cairo_paint(cr);
}

}  // namespace gfx
