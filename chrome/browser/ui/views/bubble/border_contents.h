// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_BORDER_CONTENTS_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_BORDER_CONTENTS_H_
#pragma once

#include "chrome/browser/ui/views/bubble/bubble_border.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/view.h"

// This is used to paint the border of the Bubble. Windows uses this via
// BorderWidgetWin, while others can use it directly in the bubble.
class BorderContents : public views::View {
 public:
  BorderContents() : bubble_border_(NULL) { }

  // Must be called before this object can be used.
  void Init();

  // Sets the background color.
  void SetBackgroundColor(SkColor color);

  // Given the size of the contents and the rect to point at, returns the bounds
  // of both the border and the contents inside the bubble.
  // |arrow_location| specifies the preferred location for the arrow
  // anchor. If the bubble does not fit on the monitor and
  // |allow_bubble_offscreen| is false, the arrow location may change so the
  // bubble shows entirely.
  virtual void SizeAndGetBounds(
      const gfx::Rect& position_relative_to,  // In screen coordinates
      BubbleBorder::ArrowLocation arrow_location,
      bool allow_bubble_offscreen,
      const gfx::Size& contents_size,
      gfx::Rect* contents_bounds,             // Returned in window coordinates
      gfx::Rect* window_bounds);              // Returned in screen coordinates

 protected:
  virtual ~BorderContents() { }

  // Returns the bounds for the monitor showing the specified |rect|.
  // Overridden in unit-tests.
  virtual gfx::Rect GetMonitorBounds(const gfx::Rect& rect);

  // Margins between the contents and the inside of the border, in pixels.
  static const int kLeftMargin = 6;
  static const int kTopMargin = 6;
  static const int kRightMargin = 6;
  static const int kBottomMargin = 9;

  BubbleBorder* bubble_border_;

 private:
  // Changes |arrow_location| to its mirrored version, vertically if |vertical|
  // is true, horizontally otherwise, if |window_bounds| don't fit in
  // |monitor_bounds|.
  void MirrorArrowIfOffScreen(bool vertical,
                              const gfx::Rect& position_relative_to,
                              const gfx::Rect& monitor_bounds,
                              const gfx::Size& local_contents_size,
                              BubbleBorder::ArrowLocation* arrow_location,
                              gfx::Rect* window_bounds);

  // Computes how much |window_bounds| is off-screen of the monitor bounds
  // |monitor_bounds| and puts the values in |offscreen_insets|.
  // Returns false if |window_bounds| is actually contained in |monitor_bounds|,
  // in which case |offscreen_insets| is not modified.
  static bool ComputeOffScreenInsets(const gfx::Rect& monitor_bounds,
                                     const gfx::Rect& window_bounds,
                                     gfx::Insets* offscreen_insets);

  // Convenience method that returns the height of |insets| if |vertical| is
  // true, its width otherwise.
  static int GetInsetsLength(const gfx::Insets& insets, bool vertical);

  DISALLOW_COPY_AND_ASSIGN(BorderContents);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_BORDER_CONTENTS_H_
