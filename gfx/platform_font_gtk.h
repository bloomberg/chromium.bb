// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GFX_PLATFORM_FONT_GTK_
#define GFX_PLATFORM_FONT_GTK_
#pragma once

#include "base/scoped_ptr.h"
#include "gfx/platform_font.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkTypeface;
class SkPaint;

namespace gfx {

class PlatformFontGtk : public PlatformFont {
 public:
  PlatformFontGtk();
  explicit PlatformFontGtk(const Font& other);
  explicit PlatformFontGtk(NativeFont native_font);
  PlatformFontGtk(const string16& font_name,
                  int font_size);

  // Converts |gfx_font| to a new pango font. Free the returned font with
  // pango_font_description_free().
  static PangoFontDescription* PangoFontFromGfxFont(const gfx::Font& gfx_font);

  // Position as an offset from the height of the drawn text, used to draw
  // an underline. This is a negative number, so the underline would be
  // drawn at y + height + underline_position;
  double underline_position() const;
  // The thickness to draw the underline.
  double underline_thickness() const;

  // Overridden from PlatformFont:
  virtual Font DeriveFont(int size_delta, int style) const;
  virtual int GetHeight() const;
  virtual int GetBaseline() const;
  virtual int GetAverageCharacterWidth() const;
  virtual int GetStringWidth(const string16& text) const;
  virtual int GetExpectedTextWidth(int length) const;
  virtual int GetStyle() const;
  virtual string16 GetFontName() const;
  virtual int GetFontSize() const;
  virtual NativeFont GetNativeFont() const;

 private:
  // Create a new instance of this object with the specified properties. Called
  // from DeriveFont.
  PlatformFontGtk(SkTypeface* typeface,
                  const string16& name,
                  int size,
                  int style);
  virtual ~PlatformFontGtk();

  // Initialize this object.
  void InitWithNameAndSize(const string16& font_name, int font_size);
  void InitWithTypefaceNameSizeAndStyle(SkTypeface* typeface,
                                        const string16& name,
                                        int size,
                                        int style);
  void InitFromPlatformFont(const PlatformFontGtk* other);

  // Potentially slow call to get pango metrics (average width, underline info).
  void InitPangoMetrics();

  // Setup a Skia context to use the current typeface
  void PaintSetup(SkPaint* paint) const;

  // Make |this| a copy of |other|.
  void CopyFont(const Font& other);

  // The average width of a character, initialized and cached if needed.
  double GetAverageWidth() const;

  // These two both point to the same SkTypeface. We use the SkAutoUnref to
  // handle the reference counting, but without @typeface_ we would have to
  // cast the SkRefCnt from @typeface_helper_ every time.
  scoped_ptr<SkAutoUnref> typeface_helper_;
  SkTypeface *typeface_;

  // Additional information about the face
  // Skia actually expects a family name and not a font name.
  string16 font_family_;
  int font_size_pixels_;
  int style_;

  // Cached metrics, generated at construction
  int height_pixels_;
  int ascent_pixels_;

  // The pango metrics are much more expensive so we wait until we need them
  // to compute them.
  bool pango_metrics_inited_;
  double average_width_pixels_;
  double underline_position_pixels_;
  double underline_thickness_pixels_;

  // The default font, used for the default constructor.
  static Font* default_font_;
};

}  // namespace gfx

#endif  // GFX_PLATFORM_FONT_GTK_
