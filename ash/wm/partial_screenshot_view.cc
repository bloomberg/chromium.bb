// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/partial_screenshot_view.h"

#include <algorithm>

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overlay_event_filter.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

// A self-owned object to handle the cancel and the finish of current partial
// screenshot session.
class PartialScreenshotView::OverlayDelegate
    : public OverlayEventFilter::Delegate,
      public views::WidgetObserver {
 public:
  OverlayDelegate() {
    Shell::GetInstance()->overlay_filter()->Activate(this);
  }

  void RegisterWidget(views::Widget* widget) {
    widgets_.push_back(widget);
    widget->AddObserver(this);
  }

  // Overridden from OverlayEventFilter::Delegate:
  virtual void Cancel() OVERRIDE {
    // Make sure the mouse_warp_mode allows warping. It can be stopped by a
    // partial screenshot view.
    MouseCursorEventFilter* mouse_cursor_filter =
        Shell::GetInstance()->mouse_cursor_filter();
    mouse_cursor_filter->set_mouse_warp_mode(
        MouseCursorEventFilter::WARP_ALWAYS);
    for (size_t i = 0; i < widgets_.size(); ++i)
      widgets_[i]->Close();
  }

  virtual bool IsCancelingKeyEvent(ui::KeyEvent* event) OVERRIDE {
    return event->key_code() == ui::VKEY_ESCAPE;
  }

  virtual aura::Window* GetWindow() OVERRIDE {
    // Just returns NULL because this class does not handle key events in
    // OverlayEventFilter, except for cancel keys.
    return NULL;
  }

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    widget->RemoveObserver(this);
    widgets_.erase(std::remove(widgets_.begin(), widgets_.end(), widget));
    if (widgets_.empty())
      delete this;
  }

 private:
  virtual ~OverlayDelegate() {
    Shell::GetInstance()->overlay_filter()->Deactivate();
  }

  std::vector<views::Widget*> widgets_;

  DISALLOW_COPY_AND_ASSIGN(OverlayDelegate);
};

// static
std::vector<PartialScreenshotView*>
PartialScreenshotView::StartPartialScreenshot(
    ScreenshotDelegate* screenshot_delegate) {
  std::vector<PartialScreenshotView*> views;
  OverlayDelegate* overlay_delegate = new OverlayDelegate();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    PartialScreenshotView* new_view = new PartialScreenshotView(
        overlay_delegate, screenshot_delegate);
    new_view->Init(*it);
    views.push_back(new_view);
  }
  return views;
}

PartialScreenshotView::PartialScreenshotView(
    PartialScreenshotView::OverlayDelegate* overlay_delegate,
    ScreenshotDelegate* screenshot_delegate)
    : is_dragging_(false),
      overlay_delegate_(overlay_delegate),
      screenshot_delegate_(screenshot_delegate) {
}

PartialScreenshotView::~PartialScreenshotView() {
  overlay_delegate_ = NULL;
  screenshot_delegate_ = NULL;
}

void PartialScreenshotView::Init(aura::Window* root_window) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.delegate = this;
  // The partial screenshot rectangle has to be at the real top of
  // the screen.
  params.parent =
      Shell::GetContainer(root_window, kShellWindowId_OverlayContainer);

  widget->Init(params);
  widget->SetContentsView(this);
  widget->SetBounds(root_window->GetBoundsInScreen());
  widget->GetNativeView()->SetName("PartialScreenshotView");
  widget->StackAtTop();
  widget->Show();
  // Releases the mouse capture to let mouse events come to the view. This
  // will close the context menu.
  aura::client::CaptureClient* capture_client =
      aura::client::GetCaptureClient(root_window);
  if (capture_client->GetCaptureWindow())
    capture_client->ReleaseCapture(capture_client->GetCaptureWindow());

  overlay_delegate_->RegisterWidget(widget);
}

gfx::Rect PartialScreenshotView::GetScreenshotRect() const {
  int left = std::min(start_position_.x(), current_position_.x());
  int top = std::min(start_position_.y(), current_position_.y());
  int width = ::abs(start_position_.x() - current_position_.x());
  int height = ::abs(start_position_.y() - current_position_.y());
  return gfx::Rect(left, top, width, height);
}

void PartialScreenshotView::OnSelectionStarted(const gfx::Point& position) {
  start_position_ = position;
}

void PartialScreenshotView::OnSelectionChanged(const gfx::Point& position) {
  if (is_dragging_ && current_position_ == position)
    return;
  current_position_ = position;
  SchedulePaint();
  is_dragging_ = true;
}

void PartialScreenshotView::OnSelectionFinished() {
  overlay_delegate_->Cancel();
  if (!is_dragging_)
    return;

  is_dragging_ = false;
  if (screenshot_delegate_) {
    aura::Window*root_window =
        GetWidget()->GetNativeWindow()->GetRootWindow();
    screenshot_delegate_->HandleTakePartialScreenshot(
        root_window,
        gfx::IntersectRects(root_window->bounds(), GetScreenshotRect()));
  }
}

gfx::NativeCursor PartialScreenshotView::GetCursor(
    const ui::MouseEvent& event) {
  // Always use "crosshair" cursor.
  return ui::kCursorCross;
}

void PartialScreenshotView::OnPaint(gfx::Canvas* canvas) {
  if (is_dragging_) {
    // Screenshot area representation: black rectangle with white
    // rectangle inside.  To avoid capturing these rectangles when mouse
    // release, they should be outside of the actual capturing area.
    gfx::Rect screenshot_rect = GetScreenshotRect();
    screenshot_rect.Inset(-1, -1, -1, -1);
    canvas->DrawRect(screenshot_rect, SK_ColorWHITE);
    screenshot_rect.Inset(-1, -1, -1, -1);
    canvas->DrawRect(screenshot_rect, SK_ColorBLACK);
  }
}

bool PartialScreenshotView::OnMousePressed(const ui::MouseEvent& event) {
  // Prevent moving across displays during drag. Capturing a screenshot across
  // the displays is not supported yet.
  // TODO(mukai): remove this restriction.
  MouseCursorEventFilter* mouse_cursor_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  mouse_cursor_filter->set_mouse_warp_mode(MouseCursorEventFilter::WARP_NONE);
  OnSelectionStarted(event.location());
  return true;
}

bool PartialScreenshotView::OnMouseDragged(const ui::MouseEvent& event) {
  OnSelectionChanged(event.location());
  return true;
}

bool PartialScreenshotView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // Do nothing but do not propagate events futhermore.
  return true;
}

void PartialScreenshotView::OnMouseReleased(const ui::MouseEvent& event) {
  OnSelectionFinished();
}

void PartialScreenshotView::OnMouseCaptureLost() {
  is_dragging_ = false;
  OnSelectionFinished();
}

void PartialScreenshotView::OnGestureEvent(ui::GestureEvent* event) {
  switch(event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      OnSelectionStarted(event->location());
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      OnSelectionChanged(event->location());
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      OnSelectionFinished();
      break;
    default:
      break;
  }

  event->SetHandled();
}

}  // namespace ash
