// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_FRAME_HEADER_PAINTER_H_
#define MASH_WM_FRAME_HEADER_PAINTER_H_

namespace gfx {
class Canvas;
}

namespace mash {
namespace wm {

// Helper class for painting the window header.
// TODO(sky): keep this only if we're going to actually need different
// subclasses.
class HeaderPainter {
 public:
  enum Mode { MODE_ACTIVE, MODE_INACTIVE };

  virtual ~HeaderPainter() {}

  // Returns the header's minimum width.
  virtual int GetMinimumHeaderWidth() const = 0;

  // Paints the header.
  virtual void PaintHeader(gfx::Canvas* canvas, Mode mode) = 0;

  // Performs layout for the header.
  virtual void LayoutHeader() = 0;

  // Get the height of the header.
  virtual int GetHeaderHeight() const = 0;

  // Gets / sets how much of the header is painted. This allows the header to
  // paint under things (like the tabstrip) which have transparent /
  // non-painting sections. This height does not affect LayoutHeader().
  virtual int GetHeaderHeightForPainting() const = 0;
  virtual void SetHeaderHeightForPainting(int height_for_painting) = 0;

  // Schedule a re-paint of the entire title.
  virtual void SchedulePaintForTitle() = 0;

  // Updates the x inset of the leftmost view in the header.
  virtual void UpdateLeftViewXInset(int left_view_x_inset) = 0;
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_FRAME_HEADER_PAINTER_H_
