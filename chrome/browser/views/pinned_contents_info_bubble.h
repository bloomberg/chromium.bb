// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_PINNED_CONTENTS_INFO_BUBBLE_H_
#define CHROME_BROWSER_VIEWS_PINNED_CONTENTS_INFO_BUBBLE_H_

#include "chrome/browser/views/info_bubble.h"

// This is a specialization of BorderContents, used to draw a border around
// an InfoBubble that has its contents pinned to a specific location. See
// base class for details.
class PinnedContentsBorderContents : public BorderContents {
 public:
  explicit PinnedContentsBorderContents(const gfx::Point& bubble_anchor)
      : bubble_anchor_(bubble_anchor) {}

  // BorderContents overrides:
  virtual void SizeAndGetBounds(
      const gfx::Rect& position_relative_to,  // In screen coordinates
      const gfx::Size& contents_size,
      bool prefer_arrow_on_right,
      gfx::Rect* contents_bounds,             // Returned in window coordinates
      gfx::Rect* window_bounds);              // Returned in screen coordinates

 private:
  // The location of the pinned contents (in screen coordinates).
  const gfx::Point bubble_anchor_;

  DISALLOW_COPY_AND_ASSIGN(PinnedContentsBorderContents);
};

#if defined(OS_WIN)
// The window that surrounds the info bubble. See base class for details.
class PinnedContentsBorderWidget : public BorderWidget {
 public:
  explicit PinnedContentsBorderWidget(const gfx::Point& bubble_anchor)
     : bubble_anchor_(bubble_anchor) {}
  virtual ~PinnedContentsBorderWidget() {}

  // BorderWidget overrides:
  virtual BorderContents* CreateBorderContents();

 private:
  // The location of the pinned contents (in screen coordinates).
  const gfx::Point bubble_anchor_;

  DISALLOW_COPY_AND_ASSIGN(PinnedContentsBorderWidget);
};
#endif

// A specialization of the InfoBubble. Used to draw an InfoBubble which, in
// addition to having an arrow pointing to where the user clicked, also shifts
// the bubble horizontally to fix it to a specific location. See base class
// for details.
class PinnedContentsInfoBubble : public InfoBubble {
 public:
  // Shows the InfoBubble (see base class function for details).
  // |bubble_anchor| specifies how far horizontally to shift the bubble in
  // order to anchor its contents. Once the InfoBubble has been anchored its
  // arrow may be pointing to a slightly different |y| location than specified
  // in |position_relative_to|.
  static PinnedContentsInfoBubble* Show(views::Window* parent,
                                        const gfx::Rect& position_relative_to,
                                        const gfx::Point& bubble_anchor_,
                                        views::View* contents,
                                        InfoBubbleDelegate* delegate);

 private:
  explicit PinnedContentsInfoBubble(const gfx::Point& bubble_anchor)
      : bubble_anchor_(bubble_anchor) {}
  virtual ~PinnedContentsInfoBubble() {}

  // InfoBubble overrides:
#if defined(OS_WIN)
  virtual BorderWidget* CreateBorderWidget();
#endif

  // The location of the pinned contents (in screen coordinates).
  const gfx::Point bubble_anchor_;

  DISALLOW_COPY_AND_ASSIGN(PinnedContentsInfoBubble);
};

#endif  // CHROME_BROWSER_VIEWS_PINNED_CONTENTS_INFO_BUBBLE_H_
