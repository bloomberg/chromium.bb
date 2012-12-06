// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/partial_screenshot_view.h"

#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overlay_event_filter.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/root_window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
class PartialScreenshotOverlayDelegate
    : public internal::OverlayEventFilter::Delegate {
 public:
  PartialScreenshotOverlayDelegate() {
    Shell::GetInstance()->overlay_filter()->Activate(this);
  }

  virtual ~PartialScreenshotOverlayDelegate() {
    Shell::GetInstance()->overlay_filter()->Deactivate();
  }

  void CreatePartialScreenshotUI(ScreenshotDelegate* screenshot_delegate,
                                 aura::RootWindow* root_window) {
    views::Widget* widget = new views::Widget;
    PartialScreenshotView* view = new PartialScreenshotView(
        this, screenshot_delegate);
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.transparent = true;
    params.delegate = view;
    // The partial screenshot rectangle has to be at the real top of
    // the screen.
    params.parent = Shell::GetContainer(
        root_window,
        internal::kShellWindowId_OverlayContainer);

    widget->Init(params);
    widget->SetContentsView(view);
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

    widgets_.push_back(widget);
  }

  // OverlayEventFilter::Delegate overrides:
  void Cancel() OVERRIDE {
    // Make sure the mouse_warp_mode allows warping. It can be stopped by a
    // partial screenshot view.
    internal::MouseCursorEventFilter* mouse_cursor_filter =
        Shell::GetInstance()->mouse_cursor_filter();
    mouse_cursor_filter->set_mouse_warp_mode(
        internal::MouseCursorEventFilter::WARP_ALWAYS);
    for (size_t i = 0; i < widgets_.size(); ++i)
      widgets_[i]->Close();
    delete this;
  }

  bool IsCancelingKeyEvent(ui::KeyEvent* event) OVERRIDE {
    return event->key_code() == ui::VKEY_ESCAPE;
  }

  aura::Window* GetWindow() OVERRIDE {
    // Just returns NULL because this class does not handle key events in
    // OverlayEventFilter, except for cancel keys.
    return NULL;
  }

 private:
  std::vector<views::Widget*> widgets_;

  DISALLOW_COPY_AND_ASSIGN(PartialScreenshotOverlayDelegate);
};
}

PartialScreenshotView::PartialScreenshotView(
    internal::OverlayEventFilter::Delegate* overlay_delegate,
    ScreenshotDelegate* screenshot_delegate)
    : is_dragging_(false),
      overlay_delegate_(overlay_delegate),
      screenshot_delegate_(screenshot_delegate) {
}

PartialScreenshotView::~PartialScreenshotView() {
  screenshot_delegate_ = NULL;
}

// static
void PartialScreenshotView::StartPartialScreenshot(
    ScreenshotDelegate* screenshot_delegate) {
  PartialScreenshotOverlayDelegate* overlay_delegate =
      new PartialScreenshotOverlayDelegate();
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (Shell::RootWindowList::iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    overlay_delegate->CreatePartialScreenshotUI(screenshot_delegate, *it);
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
  internal::MouseCursorEventFilter* mouse_cursor_filter =
      Shell::GetInstance()->mouse_cursor_filter();
  mouse_cursor_filter->set_mouse_warp_mode(
      internal::MouseCursorEventFilter::WARP_NONE);
  start_position_ = event.location();
  return true;
}

bool PartialScreenshotView::OnMouseDragged(const ui::MouseEvent& event) {
  current_position_ = event.location();
  SchedulePaint();
  is_dragging_ = true;
  return true;
}

bool PartialScreenshotView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // Do nothing but do not propagate events futhermore.
  return true;
}

void PartialScreenshotView::OnMouseReleased(const ui::MouseEvent& event) {
  overlay_delegate_->Cancel();
  if (!is_dragging_)
    return;

  is_dragging_ = false;
  if (screenshot_delegate_) {
    aura::RootWindow *root_window =
        GetWidget()->GetNativeWindow()->GetRootWindow();
    screenshot_delegate_->HandleTakePartialScreenshot(
        root_window,
        gfx::IntersectRects(root_window->bounds(), GetScreenshotRect()));
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
