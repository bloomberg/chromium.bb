// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/pinned_contents_info_bubble.h"

#include "chrome/browser/views/bubble_border.h"
#include "views/window/window.h"

#if defined(OS_WIN)
// BorderWidget ---------------------------------------------------------------

void PinnedContentsBorderContents::SizeAndGetBounds(
    const gfx::Rect& position_relative_to,
    const gfx::Size& contents_size,
    bool prefer_arrow_on_right,
    gfx::Rect* contents_bounds,
    gfx::Rect* window_bounds) {
  // Arrow offset is calculated from the middle of the |position_relative_to|.
  int offset = position_relative_to.x() + (position_relative_to.width() / 2);
  offset -= bubble_anchor_.x();

  gfx::Insets insets;
  bubble_border_->GetInsets(&insets);
  offset += kLeftMargin + insets.left() + 1;
  bubble_border_->set_arrow_offset(offset);

  BorderContents::SizeAndGetBounds(
      position_relative_to, contents_size, prefer_arrow_on_right,
      contents_bounds, window_bounds);

  // Now move the y position to make sure the bubble contents overlap the view.
  window_bounds->Offset(0, -(kTopMargin + 1));
}

BorderContents* PinnedContentsBorderWidget::CreateBorderContents() {
  return new PinnedContentsBorderContents(bubble_anchor_);
}
#endif

// InfoBubble -----------------------------------------------------------------

// static
PinnedContentsInfoBubble* PinnedContentsInfoBubble::Show(
    views::Window* parent,
    const gfx::Rect& position_relative_to,
    const gfx::Point& bubble_anchor,
    views::View* contents,
    InfoBubbleDelegate* delegate) {
  PinnedContentsInfoBubble* window =
      new PinnedContentsInfoBubble(bubble_anchor);
  window->Init(parent, position_relative_to, contents, delegate);
  return window;
}

// TODO(finnur): This needs to be implemented for other platforms once we decide
// this is the way to go.
#if defined(OS_WIN)
BorderWidget* PinnedContentsInfoBubble::CreateBorderWidget() {
  return new PinnedContentsBorderWidget(bubble_anchor_);
}
#endif
