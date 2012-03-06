// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FRAME_PAINTER_H_
#define ASH_WM_FRAME_PAINTER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"

class SkBitmap;
namespace gfx {
class Canvas;
class Font;
class Rect;
class Point;
class Size;
}
namespace views {
class ImageButton;
class NonClientFrameView;
class View;
class Widget;
}

namespace ash {

// Helper class for painting window frames.  Exists to share code between
// various implementations of views::NonClientFrameView.
class ASH_EXPORT FramePainter {
 public:
  FramePainter();
  ~FramePainter();

  // |frame| and buttons are used for layout and are not owned.
  void Init(views::Widget* frame,
            views::View* window_icon,
            views::ImageButton* maximize_button,
            views::ImageButton* close_button);

  // Helpers for views::NonClientFrameView implementations.
  gfx::Rect GetBoundsForClientView(int top_height,
                                   const gfx::Rect& window_bounds) const;
  gfx::Rect GetWindowBoundsForClientBounds(
      int top_height,
      const gfx::Rect& client_bounds) const;
  int NonClientHitTest(views::NonClientFrameView* view,
                       const gfx::Point& point);

  // Paints the frame header.
  void PaintHeader(views::NonClientFrameView* view,
                   gfx::Canvas* canvas,
                   const SkBitmap* theme_frame,
                   const SkBitmap* theme_frame_overlay);

  // Paint the title bar, primarily the title string.
  void PaintTitleBar(views::NonClientFrameView* view,
                     gfx::Canvas* canvas,
                     const gfx::Font& title_font);

  // Performs layout for the header based on whether we want the shorter
  // |maximized_layout| appearance.
  void LayoutHeader(views::NonClientFrameView* view, bool maximized_layout);

 private:
  // Sets the images for a button base on IDs from the |frame_| theme provider.
  void SetButtonImages(views::ImageButton* button,
                       int normal_bitmap_id,
                       int hot_bitmap_id,
                       int pushed_bitmap_id);

  // Not owned
  views::Widget* frame_;
  views::View* window_icon_;  // May be NULL.
  views::ImageButton* maximize_button_;
  views::ImageButton* close_button_;

  // Window frame header/caption parts.
  const SkBitmap* button_separator_;
  const SkBitmap* top_left_corner_;
  const SkBitmap* top_edge_;
  const SkBitmap* top_right_corner_;
  const SkBitmap* header_left_edge_;
  const SkBitmap* header_right_edge_;

  DISALLOW_COPY_AND_ASSIGN(FramePainter);
};

}  // namespace ash

#endif  // ASH_WM_FRAME_PAINTER_H_
