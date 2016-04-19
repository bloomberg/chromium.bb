// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_INFO_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_INFO_BUBBLE_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

namespace autofill {

class InfoBubbleFrame;

// Class to create and manage an information bubble for errors or tooltips.
class InfoBubble : public views::BubbleDialogDelegateView {
 public:
  InfoBubble(views::View* anchor, const base::string16& message);
  ~InfoBubble() override;

  // Shows the bubble. |widget_| will be NULL until this is called.
  void Show();

  // Hides and closes the bubble.
  void Hide();

  // Updates the position of the bubble.
  void UpdatePosition();

  // views::BubbleDialogDelegateView:
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  gfx::Size GetPreferredSize() const override;
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  int GetDialogButtons() const override;

  views::View* anchor() { return anchor_; }
  const views::View* anchor() const { return anchor_; }

  void set_align_to_anchor_edge(bool align_to_anchor_edge) {
    align_to_anchor_edge_ = align_to_anchor_edge;
  }

  void set_preferred_width(int preferred_width) {
    preferred_width_ = preferred_width;
  }

  void set_show_above_anchor(bool show_above_anchor) {
    show_above_anchor_ = show_above_anchor;
  }

 private:
  views::Widget* widget_;  // Weak, may be NULL.
  views::View* const anchor_;  // Weak.
  InfoBubbleFrame* frame_;  // Weak, owned by widget.

  // Whether the bubble should align its border to the anchor's edge rather than
  // horizontally centering the arrow on |anchor_|'s midpoint. Default is false.
  bool align_to_anchor_edge_;

  // The width this bubble prefers to be. Default is 0 (no preference).
  int preferred_width_;

  // Whether the bubble should be shown above the anchor (default is below).
  bool show_above_anchor_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubble);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_INFO_BUBBLE_H_
