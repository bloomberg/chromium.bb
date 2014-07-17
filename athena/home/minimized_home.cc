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

class MinimizedHomeView : public views::View {
 public:
  explicit MinimizedHomeView(athena::MinimizedHomeDragDelegate* delegate)
      : delegate_(delegate),
        color_(SK_ColorTRANSPARENT) {
    SetColor(kDragHandleColorNormal);
  }
  virtual ~MinimizedHomeView() {}

 private:
  void SetColor(SkColor color) {
    if (color_ == color)
      return;
    color_ = color;
    set_background(views::Background::CreateSolidBackground(color_));
    SchedulePaint();
  }

  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    const int kDragHandleWidth = 80;
    const int kDragHandleHeight = 4;
    return gfx::Size(kDragHandleWidth, kDragHandleHeight);
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    SkColor change_color = SK_ColorTRANSPARENT;
    if (event->type() == ui::ET_GESTURE_BEGIN &&
        event->details().touch_points() == 1) {
      change_color = kDragHandleColorHot;
    } else if (event->type() == ui::ET_GESTURE_END &&
               event->details().touch_points() == 1) {
      change_color = kDragHandleColorNormal;
    }

    if (change_color != SK_ColorTRANSPARENT) {
      SetColor(change_color);
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
      SetColor(kDragHandleColorNormal);
    }
  }

  athena::MinimizedHomeDragDelegate* delegate_;
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(MinimizedHomeView);
};

}  // namespace

namespace athena {

views::Widget* CreateMinimizedHome(aura::Window* container,
                                   MinimizedHomeDragDelegate* delegate) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = container;
  params.delegate = NULL;
  widget->Init(params);

  views::View* content_view = new views::View;
  widget->SetContentsView(content_view);
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 2, 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  content_view->SetLayoutManager(layout);

  views::View* view = new MinimizedHomeView(delegate);
  content_view->AddChildView(view);
  return widget;
}

}  // namespace athena
