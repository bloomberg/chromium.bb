// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/util/drag_handle.h"

#include "ui/views/background.h"
#include "ui/views/view.h"

namespace athena {
namespace {

const SkColor kDragHandleColorNormal = SK_ColorGRAY;
const SkColor kDragHandleColorHot = SK_ColorWHITE;

// This view notifies its delegate of the touch scroll gestures performed on it.
class DragHandleView : public views::View {
 public:
  DragHandleView(DragHandleScrollDirection scroll_direction,
                 DragHandleScrollDelegate* delegate,
                 int preferred_width,
                 int preferred_height);
  virtual ~DragHandleView();

 private:
  void SetColor(SkColor color);

  void SetIsScrolling(bool scrolling);

  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  bool scroll_in_progress_;
  DragHandleScrollDelegate* delegate_;
  DragHandleScrollDirection scroll_direction_;
  SkColor color_;
  float scroll_start_location_;
  const int preferred_width_;
  const int preferred_height_;

  DISALLOW_COPY_AND_ASSIGN(DragHandleView);
};

DragHandleView::DragHandleView(DragHandleScrollDirection scroll_direction,
                               DragHandleScrollDelegate* delegate,
                               int preferred_width,
                               int preferred_height)
    : scroll_in_progress_(false),
      delegate_(delegate),
      scroll_direction_(scroll_direction),
      color_(SK_ColorTRANSPARENT),
      preferred_width_(preferred_width),
      preferred_height_(preferred_height) {
  SetColor(kDragHandleColorNormal);
}

DragHandleView::~DragHandleView() {
}

void DragHandleView::SetColor(SkColor color) {
  if (color_ == color)
    return;
  color_ = color;
  set_background(views::Background::CreateSolidBackground(color_));
  SchedulePaint();
}

void DragHandleView::SetIsScrolling(bool scrolling) {
  if (scroll_in_progress_ == scrolling)
    return;
  scroll_in_progress_ = scrolling;
  if (!scroll_in_progress_)
    scroll_start_location_ = 0;
}

// views::View:
gfx::Size DragHandleView::GetPreferredSize() const {
  return gfx::Size(preferred_width_, preferred_height_);
}

void DragHandleView::OnGestureEvent(ui::GestureEvent* event) {
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
    if (scroll_in_progress_)
      return;
    float delta;
    if (scroll_direction_ == DRAG_HANDLE_VERTICAL) {
      delta = event->details().scroll_y_hint();
      scroll_start_location_ = event->root_location().y();
    } else {
      delta = event->details().scroll_x_hint();
      scroll_start_location_ = event->root_location().x();
    }
    delegate_->HandleScrollBegin(delta);
    SetIsScrolling(true);
    event->SetHandled();
  } else if (event->type() == ui::ET_GESTURE_SCROLL_END ||
             event->type() == ui::ET_SCROLL_FLING_START) {
    if (!scroll_in_progress_)
      return;
    delegate_->HandleScrollEnd();
    SetColor(kDragHandleColorNormal);
    SetIsScrolling(false);
    event->SetHandled();
  } else if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    if (!scroll_in_progress_)
      return;
    float delta = scroll_direction_ == DRAG_HANDLE_VERTICAL
                      ? event->root_location().y() - scroll_start_location_
                      : event->root_location().x() - scroll_start_location_;
    delegate_->HandleScrollUpdate(delta);
    event->SetHandled();
  }
}

}  // namespace

views::View* CreateDragHandleView(DragHandleScrollDirection scroll_direction,
                                  DragHandleScrollDelegate* delegate,
                                  int preferred_width,
                                  int preferred_height) {
  views::View* view = new DragHandleView(
      scroll_direction, delegate, preferred_width, preferred_height);
  return view;
}

}  // namespace athena
