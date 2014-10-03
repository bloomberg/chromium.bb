// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_
#define ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/bubble/bubble_delegate.h"

namespace ash {
class ShelfLayoutManager;
class ShelfView;

namespace test {
class OverflowBubbleViewTestAPI;
}

// OverflowBubbleView hosts a ShelfView to display overflown items.
// Exports to access this class from OverflowBubbleViewTestAPI.
class ASH_EXPORT OverflowBubbleView : public views::BubbleDelegateView {
 public:
  OverflowBubbleView();
  virtual ~OverflowBubbleView();

  void InitOverflowBubble(views::View* anchor, ShelfView* shelf_view);

  // views::BubbleDelegateView overrides:
  virtual gfx::Rect GetBubbleBounds() override;

 private:
  friend class test::OverflowBubbleViewTestAPI;

  bool IsHorizontalAlignment() const;

  const gfx::Size GetContentsSize() const;

  // Gets arrow location based on shelf alignment.
  views::BubbleBorder::Arrow GetBubbleArrow() const;

  void ScrollByXOffset(int x_offset);
  void ScrollByYOffset(int y_offset);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() const override;
  virtual void Layout() override;
  virtual void ChildPreferredSizeChanged(views::View* child) override;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  // ui::EventHandler overrides:
  virtual void OnScrollEvent(ui::ScrollEvent* event) override;

  ShelfLayoutManager* GetShelfLayoutManager() const;

  ShelfView* shelf_view_;  // Owned by views hierarchy.
  gfx::Vector2d scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(OverflowBubbleView);
};

}  // namespace ash

#endif  // ASH_SHELF_OVERFLOW_BUBBLE_VIEW_H_
