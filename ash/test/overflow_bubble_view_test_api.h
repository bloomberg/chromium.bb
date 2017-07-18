// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_OVERFLOW_BUBBLE_VIEW_TEST_API_
#define ASH_TEST_OVERFLOW_BUBBLE_VIEW_TEST_API_

#include "base/macros.h"

namespace gfx {
class Size;
}

namespace views {
class BubbleFrameView;
}

namespace ash {
class OverflowBubbleView;

namespace test {

class OverflowBubbleViewTestAPI {
 public:
  explicit OverflowBubbleViewTestAPI(OverflowBubbleView* bubble_view);
  ~OverflowBubbleViewTestAPI();

  // Returns the total width of items included in ShelfView.
  gfx::Size GetContentsSize();

  // Emulates scroll operations on OverflowBubble to make invisible last item
  // visible.
  void ScrollByXOffset(int x_offset);

  // Returns the NonClientFrameView for the bubble.
  views::BubbleFrameView* GetBubbleFrameView();

 private:
  OverflowBubbleView* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(OverflowBubbleViewTestAPI);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_OVERFLOW_BUBBLE_VIEW_TEST_API_
