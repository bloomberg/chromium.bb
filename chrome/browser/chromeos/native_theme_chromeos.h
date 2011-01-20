// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NATIVE_THEME_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_NATIVE_THEME_CHROMEOS_H_

#include <map>
#include "gfx/native_theme_linux.h"

class SkBitmap;

class NativeThemeChromeos : public gfx::NativeThemeLinux {
 private:
  friend class NativeThemeLinux;
  NativeThemeChromeos();
  virtual ~NativeThemeChromeos();

  // Scrollbar painting overrides
  virtual gfx::Size GetPartSize(Part part) const;
  virtual void PaintScrollbarTrack(skia::PlatformCanvas* canvas,
      Part part, State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect);
  virtual void PaintArrowButton(skia::PlatformCanvas* canvas,
      const gfx::Rect& rect, Part direction, State state);
  virtual void PaintScrollbarThumb(skia::PlatformCanvas* canvas,
      Part part, State state, const gfx::Rect& rect);
  SkBitmap* GetHorizontalBitmapNamed(int resource_id);

  // Cached images. The ResourceBundle caches all retrieved bitmaps and keeps
  // ownership of the pointers.
  typedef std::map<int, SkBitmap*> SkImageMap;
  SkImageMap horizontal_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeChromeos);
};

#endif  // CHROME_BROWSER_CHROMEOS_NATIVE_THEME_CHROMEOS_H_
