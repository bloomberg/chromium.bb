// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/partial_screenshot_view.h"

#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/partial_screenshot_event_filter.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

PartialScreenshotView::PartialScreenshotView(
    ScreenshotDelegate* screenshot_delegate)
    : is_dragging_(false),
      screenshot_delegate_(screenshot_delegate),
      window_(NULL) {
}

PartialScreenshotView::~PartialScreenshotView() {
  screenshot_delegate_ = NULL;
  // Do not delete the |window_| here because |window_| has the
  // ownership to this object. In case that finishing browser happens
  // while |window_| != NULL, |window_| is still removed correctly by
  // its parent container.
  window_ = NULL;
}

// static
void PartialScreenshotView::StartPartialScreenshot(
    ScreenshotDelegate* screenshot_delegate) {
  views::Widget* widget = new views::Widget;
  PartialScreenshotView* view = new PartialScreenshotView(
      screenshot_delegate);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.delegate = view;
  // The partial screenshot rectangle has to be at the real top of
  // the screen.
  params.parent = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_OverlayContainer);

  widget->Init(params);
  widget->SetContentsView(view);
  widget->SetBounds(Shell::GetRootWindow()->bounds());
  widget->GetNativeView()->SetName("PartialScreenshotView");
  widget->StackAtTop();
  widget->Show();
  // Captures mouse events in case that context menu already captures the
  // events.  This will close the context menu.
  widget->GetNativeView()->SetCapture(ui::CW_LOCK_MOUSE | ui::CW_LOCK_TOUCH);

  view->set_window(widget->GetNativeWindow());
  Shell::GetInstance()->partial_screenshot_filter()->Activate(view);
}

void PartialScreenshotView::Cancel() {
  DCHECK(window_);
  window_->Hide();
  Shell::GetInstance()->partial_screenshot_filter()->Deactivate();
  MessageLoop::current()->DeleteSoon(FROM_HERE, window_);
}

gfx::NativeCursor PartialScreenshotView::GetCursor(
    const views::MouseEvent& event) {
  // Always use "crosshair" cursor.
  return ui::kCursorCross;
}

void PartialScreenshotView::OnPaint(gfx::Canvas* canvas) {
  if (is_dragging_) {
    // Screenshot area representation: black rectangle with white
    // rectangle inside.
    gfx::Rect screenshot_rect = GetScreenshotRect();
    canvas->DrawRect(screenshot_rect, SK_ColorBLACK);
    screenshot_rect.Inset(1, 1, 1, 1);
    canvas->DrawRect(screenshot_rect, SK_ColorWHITE);
  }
}

void PartialScreenshotView::OnMouseCaptureLost() {
  Cancel();
}

bool PartialScreenshotView::OnMousePressed(const views::MouseEvent& event) {
  start_position_ = event.location();
  return true;
}

bool PartialScreenshotView::OnMouseDragged(const views::MouseEvent& event) {
  current_position_ = event.location();
  SchedulePaint();
  is_dragging_ = true;
  return true;
}

bool PartialScreenshotView::OnMouseWheel(const views::MouseWheelEvent& event) {
  // Do nothing but do not propagate events futhermore.
  return true;
}

void PartialScreenshotView::OnMouseReleased(const views::MouseEvent& event) {
  Cancel();
  if (!is_dragging_)
    return;

  is_dragging_ = false;
  if (screenshot_delegate_) {
    aura::RootWindow *root_window = Shell::GetRootWindow();
    screenshot_delegate_->HandleTakePartialScreenshot(
        root_window, root_window->bounds().Intersect(GetScreenshotRect()));
  }
}

gfx::Rect PartialScreenshotView::GetScreenshotRect() const {
  int left = std::min(start_position_.x(), current_position_.x());
  int top = std::min(start_position_.y(), current_position_.y());
  int width = ::abs(start_position_.x() - current_position_.x());
  int height = ::abs(start_position_.y() - current_position_.y());
  return gfx::Rect(left, top, width, height);
}

}  // namespace ash
