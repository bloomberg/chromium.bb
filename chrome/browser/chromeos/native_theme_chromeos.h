// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NATIVE_THEME_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_NATIVE_THEME_CHROMEOS_H_

#include <map>
#include "base/compiler_specific.h"
#include "ui/gfx/native_theme_linux.h"

class SkBitmap;

class NativeThemeChromeos : public gfx::NativeThemeLinux {
 private:
  friend class NativeThemeLinux;
  NativeThemeChromeos();
  virtual ~NativeThemeChromeos();

  // Scrollbar painting overrides
  virtual gfx::Size GetPartSize(Part part) const OVERRIDE;
  virtual void PaintScrollbarTrack(skia::PlatformCanvas* canvas,
      Part part, State state,
      const ScrollbarTrackExtraParams& extra_params,
      const gfx::Rect& rect) OVERRIDE;
  virtual void PaintArrowButton(skia::PlatformCanvas* canvas,
      const gfx::Rect& rect, Part direction, State state) OVERRIDE;
  virtual void PaintScrollbarThumb(skia::PlatformCanvas* canvas,
      Part part, State state, const gfx::Rect& rect) OVERRIDE;

  // Draw the checkbox.
  virtual void PaintCheckbox(skia::PlatformCanvas* canvas,
      State state, const gfx::Rect& rect,
      const ButtonExtraParams& button) OVERRIDE;

  // Draw the radio.
  virtual void PaintRadio(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) OVERRIDE;

  // Draw the push button.
  virtual void PaintButton(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ButtonExtraParams& button) OVERRIDE;

  // Draw the text field.
  virtual void PaintTextField(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const TextFieldExtraParams& text) OVERRIDE;

  // Draw the slider track.
  virtual void PaintSliderTrack(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) OVERRIDE;

  // Draw the slider thumb.
  virtual void PaintSliderThumb(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const SliderExtraParams& slider) OVERRIDE;

  // Draw the inner spin button.
  virtual void PaintInnerSpinButton(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const InnerSpinButtonExtraParams& spin_button) OVERRIDE;

  // Draw the progress bar.
  virtual void PaintProgressBar(skia::PlatformCanvas* canvas,
      State state,
      const gfx::Rect& rect,
      const ProgressBarExtraParams& progress_bar) OVERRIDE;

  SkBitmap* GetHorizontalBitmapNamed(int resource_id);

  // Paint a button like rounded rect with gradient background and stroke.
  void PaintButtonLike(skia::PlatformCanvas* canvas,
      State state, const gfx::Rect& rect,
      const ButtonExtraParams& button);

  // Cached images. The ResourceBundle caches all retrieved bitmaps and keeps
  // ownership of the pointers.
  typedef std::map<int, SkBitmap*> SkImageMap;
  SkImageMap horizontal_bitmaps_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeChromeos);
};

#endif  // CHROME_BROWSER_CHROMEOS_NATIVE_THEME_CHROMEOS_H_
