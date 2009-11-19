// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAINT_AGGREGATOR_H_
#define CHROME_RENDERER_PAINT_AGGREGATOR_H_

#include "base/gfx/rect.h"

// This class is responsible for aggregating multiple invalidation and scroll
// commands to produce a single scroll and repaint.
class PaintAggregator {
 public:

  // This structure describes an aggregation of InvalidateRect and ScrollRect
  // calls.  If |scroll_rect| is non-empty, then that rect should be scrolled
  // by the amount specified by |scroll_delta|.  If |paint_rect| is non-empty,
  // then that rect should be repainted.  If |scroll_rect| and |paint_rect| are
  // non-empty, then scrolling should be performed before repainting.
  // |scroll_delta| can only specify scrolling in one direction (i.e., the x
  // and y members cannot both be non-zero).
  struct PendingUpdate {
    gfx::Point scroll_delta;
    gfx::Rect scroll_rect;
    gfx::Rect paint_rect;

    // Returns the rect damaged by scrolling within |scroll_rect| by
    // |scroll_delta|.  This rect must be repainted.
    gfx::Rect GetScrollDamage() const;
  };

  // There is a PendingUpdate if InvalidateRect or ScrollRect were called and
  // ClearPendingUpdate was not called.
  bool HasPendingUpdate() const;
  void ClearPendingUpdate();

  const PendingUpdate& GetPendingUpdate() const { return update_; }

  // The given rect should be repainted.
  void InvalidateRect(const gfx::Rect& rect);

  // The given rect should be scrolled by the given amounts.
  void ScrollRect(int dx, int dy, const gfx::Rect& clip_rect);

 private:
  PendingUpdate update_;
};

#endif  // CHROME_RENDERER_PAINT_AGGREGATOR_H_
