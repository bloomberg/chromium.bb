// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_FRAME_HEADER_PAINTER_H_
#define ASH_COMMON_FRAME_HEADER_PAINTER_H_

#include "ash/ash_export.h"

namespace gfx {
class Canvas;
}

namespace ash {

// Helper class for painting the window header.
class ASH_EXPORT HeaderPainter {
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
};

}  // namespace ash

#endif  // ASH_COMMON_FRAME_HEADER_PAINTER_H_
