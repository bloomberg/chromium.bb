// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GFX_PLATFORM_FONT_MAC_
#define GFX_PLATFORM_FONT_MAC_
#pragma once

#include "gfx/platform_font.h"

namespace gfx {

class PlatformFontMac : public PlatformFont {
 public:
  PlatformFontMac();
  explicit PlatformFontMac(const Font& other);
  explicit PlatformFontMac(NativeFont native_font);
  PlatformFontMac(const std::wstring& font_name,
                  int font_size);

  // Overridden from PlatformFont:
  virtual Font DeriveFont(int size_delta, int style) const;
  virtual int GetHeight() const;
  virtual int GetBaseline() const;
  virtual int GetAverageCharacterWidth() const;
  virtual int GetStringWidth(const std::wstring& text) const;
  virtual int GetExpectedTextWidth(int length) const;
  virtual int GetStyle() const;
  virtual const std::wstring& GetFontName() const;
  virtual int GetFontSize() const;
  virtual NativeFont GetNativeFont() const;

 private:
  PlatformFontMac(const std::wstring& font_name, int font_size, int style);
  virtual ~PlatformFontMac() {}

  // Initialize the object with the specified parameters.
  void InitWithNameSizeAndStyle(const std::wstring& font_name,
                                int font_size,
                                int style);

  // Calculate and cache the font metrics.
  void CalculateMetrics();

  std::wstring font_name_;
  int font_size_;
  int style_;

  // Cached metrics, generated at construction
  int height_;
  int ascent_;
  int average_width_;
};

}  // namespace gfx

#endif  // GFX_PLATFORM_FONT_MAC_
