// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GFX_NATIVE_THEME_LINUX_H_
#define GFX_NATIVE_THEME_LINUX_H_

#include "base/basictypes.h"
#include "skia/ext/platform_canvas.h"

namespace skia {
class PlatformCanvas;
}

namespace gfx {
class Rect;
class Size;

// Linux theming API.
class NativeThemeLinux {
 public:
  // Gets our singleton instance.
  static NativeThemeLinux* instance();

  // The part to be painted / sized.
  enum Part {
    kScrollbarDownArrow,
    kScrollbarLeftArrow,
    kScrollbarRightArrow,
    kScrollbarUpArrow,
    kScrollbarHorizontalThumb,
    kScrollbarVerticalThumb,
    kScrollbarHorizontalTrack,
    kScrollbarVerticalTrack
  };

  // The state of the part.
  enum State {
    kDisabled,
    kHover,
    kNormal,
    kPressed,
  };

  // Extra data needed to draw scrollbar track correctly.
  struct ScrollbarTrackExtraParams {
    int track_x;
    int track_y;
    int track_width;
    int track_height;
  };

  union ExtraParams {
    ScrollbarTrackExtraParams scrollbar_track;
  };

  // Return the size of the part.
  virtual gfx::Size GetSize(Part part) const;
  // Paint the part to the canvas.
  virtual void Paint(skia::PlatformCanvas* canvas,
                     Part part,
                     State state,
                     const gfx::Rect& rect,
                     const ExtraParams& extra);
  // Supports theme specific colors.
  void SetScrollbarColors(unsigned inactive_color,
                          unsigned active_color,
                          unsigned track_color) const;

 protected:
  NativeThemeLinux();
  virtual ~NativeThemeLinux();

  // Draw the arrow.
  virtual void PaintArrowButton(
      skia::PlatformCanvas* gc,
      const gfx::Rect& rect,
      Part direction,
      State state);
  // Paint the track. Done before the thumb so that it can contain alpha.
  virtual void PaintTrack(skia::PlatformCanvas* canvas,
                  Part part,
                  State state,
                  const ScrollbarTrackExtraParams& extra_params,
                  const gfx::Rect& rect);
  // Draw the thumb over the track.
  virtual void PaintThumb(skia::PlatformCanvas* canvas,
                  Part part,
                  State state,
                  const gfx::Rect& rect);

 private:
  void DrawVertLine(SkCanvas* canvas,
                    int x,
                    int y1,
                    int y2,
                    const SkPaint& paint) const;
  void DrawHorizLine(SkCanvas* canvas,
                     int x1,
                     int x2,
                     int y,
                     const SkPaint& paint) const;
  void DrawBox(SkCanvas* canvas,
               const gfx::Rect& rect,
               const SkPaint& paint) const;
  SkScalar Clamp(SkScalar value,
                 SkScalar min,
                 SkScalar max) const;
  SkColor SaturateAndBrighten(SkScalar* hsv,
                              SkScalar saturate_amount,
                              SkScalar brighten_amount) const;
  SkColor OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const;

  static unsigned int scrollbar_width_;
  static unsigned int button_length_;
  static unsigned int thumb_inactive_color_;
  static unsigned int thumb_active_color_;
  static unsigned int track_color_;

  DISALLOW_COPY_AND_ASSIGN(NativeThemeLinux);
};

}  // namespace gfx

#endif  // GFX_NATIVE_THEME_LINUX_H_
