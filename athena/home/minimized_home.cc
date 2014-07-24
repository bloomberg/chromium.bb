// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/minimized_home.h"

#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

const SkColor kDragHandleColorNormal = SK_ColorGRAY;
const SkColor kDragHandleColorHot = SK_ColorWHITE;

// The small white bar in the middle of the minimized view. Does not reach to
// events.
class SmallBarView : public views::View {
 public:
  SmallBarView() : color_(SK_ColorTRANSPARENT) {
    SetColor(kDragHandleColorNormal);
  }

  virtual ~SmallBarView() {}

  void SetActive(bool active) {
    SetColor(active ? kDragHandleColorHot : kDragHandleColorNormal);
  }

 private:
  void SetColor(SkColor color) {
    if (color_ == color)
      return;
    color_ = color;
    set_background(views::Background::CreateSolidBackground(color_));
    SchedulePaint();
  }

  // views::View
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    const int kDragHandleWidth = 80;
    const int kDragHandleHeight = 4;
    return gfx::Size(kDragHandleWidth, kDragHandleHeight);
  }

  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(SmallBarView);
};

// This View shows an instance of SmallBarView in the middle, and reacts to
// mouse and touch-gesture events.
class MinimizedHomeView : public views::View {
 public:
  explicit MinimizedHomeView(athena::MinimizedHomeDragDelegate* delegate)
      : delegate_(delegate),
        bar_(new SmallBarView) {
    set_background(views::Background::CreateSolidBackground(SK_ColorBLACK));
    views::BoxLayout* layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 2, 0);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    SetLayoutManager(layout);

    AddChildView(bar_);
  }
  virtual ~MinimizedHomeView() {}

 private:
  // views::View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    if (event.IsLeftMouseButton() && event.GetClickCount() == 1) {
      delegate_->OnDragUpCompleted();
      bar_->SetActive(false);
      return true;
    }
    return false;
  }

  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE {
    bar_->SetActive(true);
  }

  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE {
    bar_->SetActive(false);
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->type() == ui::ET_GESTURE_BEGIN &&
        event->details().touch_points() == 1) {
      bar_->SetActive(true);
      event->SetHandled();
      return;
    } else if (event->type() == ui::ET_GESTURE_END &&
               event->details().touch_points() == 1) {
      bar_->SetActive(false);
      event->SetHandled();
      return;
    }

    if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
      event->SetHandled();
    } else if (event->type() == ui::ET_SCROLL_FLING_START) {
      const ui::GestureEventDetails& details = event->details();
      const float kFlingCompletionVelocity = -100.f;
      if (details.velocity_y() < kFlingCompletionVelocity)
        delegate_->OnDragUpCompleted();
      bar_->SetActive(false);
    }
  }

  athena::MinimizedHomeDragDelegate* delegate_;
  SmallBarView* bar_;

  DISALLOW_COPY_AND_ASSIGN(MinimizedHomeView);
};

}  // namespace

namespace athena {

views::View* CreateMinimizedHome(MinimizedHomeDragDelegate* delegate) {
  return new MinimizedHomeView(delegate);
}

}  // namespace athena
