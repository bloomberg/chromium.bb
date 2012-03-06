// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/multi_window_resize_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/root_window_event_filter.h"
#include "ash/wm/workspace/workspace_event_filter.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using aura::Window;

namespace ash {
namespace internal {

namespace {

const int kShowDelayMS = 100;

// Padding from the bottom/right edge the resize widget is shown at.
const int kResizeWidgetPadding = 40;

bool ContainsX(Window* window, int x) {
  return window->bounds().x() <= x && window->bounds().right() >= x;
}

bool ContainsY(Window* window, int y) {
  return window->bounds().y() <= y && window->bounds().bottom() >= y;
}

}  // namespace

// View contained in the widget. Passes along mouse events to the
// MultiWindowResizeController so that it can start/stop the resize loop.
class MultiWindowResizeController::ResizeView : public views::View {
 public:
  explicit ResizeView(MultiWindowResizeController* controller,
                      Direction direction)
      : controller_(controller),
        direction_(direction) {
  }

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(30, 30);
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    // TODO: replace with real assets.
    SkPaint paint;
    paint.setColor(SkColorSetARGB(128, 0, 0, 0));
    paint.setStyle(SkPaint::kFill_Style);
    paint.setAntiAlias(true);
    canvas->AsCanvasSkia()->GetSkCanvas()->drawCircle(
        SkIntToScalar(15), SkIntToScalar(15), SkIntToScalar(15), paint);
  }
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->StartResize(location);
    return true;
  }
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->Resize(location);
    return true;
  }
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE {
    controller_->CompleteResize();
  }
  virtual void OnMouseCaptureLost() OVERRIDE {
    controller_->CancelResize();
  }
  virtual gfx::NativeCursor GetCursor(
      const views::MouseEvent& event) OVERRIDE {
    int component = (direction_ == LEFT_RIGHT) ? HTRIGHT : HTBOTTOM;
    return RootWindowEventFilter::CursorForWindowComponent(component);
  }

 private:
  MultiWindowResizeController* controller_;
  const Direction direction_;

  DISALLOW_COPY_AND_ASSIGN(ResizeView);
};

// MouseWatcherHost implementation for MultiWindowResizeController. Forwards
// Contains() to MultiWindowResizeController.
class MultiWindowResizeController::ResizeMouseWatcherHost :
   public views::MouseWatcherHost {
 public:
  ResizeMouseWatcherHost(MultiWindowResizeController* host) : host_(host) {}

  // MouseWatcherHost overrides:
  virtual bool Contains(const gfx::Point& screen_point,
                        MouseEventType type) OVERRIDE {
    return !host_->IsOverWindows(screen_point);
  }

 private:
  MultiWindowResizeController* host_;

  DISALLOW_COPY_AND_ASSIGN(ResizeMouseWatcherHost);
};

MultiWindowResizeController::ResizeWindows::ResizeWindows()
    : window1(NULL),
      window2(NULL),
      direction(TOP_BOTTOM){
}

MultiWindowResizeController::ResizeWindows::~ResizeWindows() {
}

bool MultiWindowResizeController::ResizeWindows::Equals(
    const ResizeWindows& other) const {
  return window1 == other.window1 &&
      window2 == other.window2 &&
      direction == other.direction;
}

MultiWindowResizeController::MultiWindowResizeController()
    : resize_widget_(NULL),
      grid_size_(0) {
}

MultiWindowResizeController::~MultiWindowResizeController() {
  Hide();
}

void MultiWindowResizeController::Show(Window* window,
                                       int component,
                                       const gfx::Point& point) {
  ResizeWindows windows(DetermineWindows(window, component, point));
  if (IsShowing()) {
    if (windows_.Equals(windows))
      return;
    Hide();
    // Fall through to see if there is another hot spot we should show at.
  }

  windows_ = windows;
  if (!windows_.is_valid())
    return;
  // TODO: need to listen for windows to be closed/destroyed, maybe window
  // moved too.
  show_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kShowDelayMS),
                    this, &MultiWindowResizeController::ShowNow);
}

void MultiWindowResizeController::Hide() {
  if (window_resizer_.get())
    return;  // Ignore hides while actively resizing.

  show_timer_.Stop();
  if (!resize_widget_)
    return;
  resize_widget_->Close();
  resize_widget_ = NULL;
  windows_ = ResizeWindows();
}

void MultiWindowResizeController::MouseMovedOutOfHost() {
  Hide();
}

MultiWindowResizeController::ResizeWindows
MultiWindowResizeController::DetermineWindows(
    Window* window,
    int window_component,
    const gfx::Point& point) const {
  ResizeWindows result;
  gfx::Point point_in_parent(point);
  Window::ConvertPointToWindow(window, window->parent(), &point_in_parent);
  switch (window_component) {
    case HTRIGHT:
      result.direction = LEFT_RIGHT;
      result.window1 = window;
      result.window2 = FindWindowByEdge(
          window, HTLEFT, window->bounds().right(), point_in_parent.y());
      break;
    case HTLEFT:
      result.direction = LEFT_RIGHT;
      result.window1 = FindWindowByEdge(
          window, HTRIGHT, window->bounds().x(), point_in_parent.y());
      result.window2 = window;
      break;
    case HTTOP:
      result.direction = TOP_BOTTOM;
      result.window1 = FindWindowByEdge(
          window, HTBOTTOM, point_in_parent.x(), window->bounds().y());
      result.window2 = window;
      break;
    case HTBOTTOM:
      result.direction = TOP_BOTTOM;
      result.window1 = window;
      result.window2 = FindWindowByEdge(
          window, HTTOP, point_in_parent.x(), window->bounds().bottom());
      break;
    default:
      break;
  }
  return result;
}

Window* MultiWindowResizeController::FindWindowByEdge(
    Window* window_to_ignore,
    int edge_want,
    int x,
    int y) const {
  Window* parent = window_to_ignore->parent();
  const Window::Windows& windows(parent->children());
  for (Window::Windows::const_reverse_iterator i = windows.rbegin();
       i != windows.rend(); ++i) {
    Window* window = *i;
    if (window == window_to_ignore || !window->IsVisible())
      continue;
    switch (edge_want) {
      case HTLEFT:
        if (ContainsY(window, y) && window->bounds().x() == x)
          return window;
        break;
      case HTRIGHT:
        if (ContainsY(window, y) && window->bounds().right() == x)
          return window;
        break;
      case HTTOP:
        if (ContainsX(window, x) && window->bounds().y() == y)
          return window;
        break;
      case HTBOTTOM:
        if (ContainsX(window, x) && window->bounds().bottom() == y)
          return window;
        break;
      default:
        NOTREACHED();
    }
    // Window doesn't contain the edge, but if window contains |point|
    // it's obscuring any other window that could be at the location.
    if (window->bounds().Contains(x, y))
      return NULL;
  }
  return NULL;
}

void MultiWindowResizeController::ShowNow() {
  DCHECK(!resize_widget_);
  DCHECK(windows_.is_valid());
  show_timer_.Stop();
  resize_widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_AlwaysOnTopContainer);
  params.can_activate = false;
  ResizeView* view = new ResizeView(this, windows_.direction);
  params.delegate = new views::WidgetDelegateView;
  resize_widget_->set_focus_on_creation(false);
  resize_widget_->Init(params);
  resize_widget_->GetNativeWindow()->SetName("MultiWindowResizeController");
  resize_widget_->SetContentsView(view);
  show_bounds_ = CalculateResizeWidgetBounds();
  resize_widget_->SetBounds(show_bounds_);
  resize_widget_->Show();
}

bool MultiWindowResizeController::IsShowing() const {
  return resize_widget_ || show_timer_.IsRunning();
}

void MultiWindowResizeController::StartResize(
    const gfx::Point& screen_location) {
  DCHECK(!window_resizer_.get());
  DCHECK(windows_.is_valid());
  gfx::Point parent_location(screen_location);
  aura::Window::ConvertPointToWindow(
      windows_.window1->GetRootWindow(), windows_.window1->parent(),
      &parent_location);
  std::vector<aura::Window*> windows;
  windows.push_back(windows_.window2);
  // TODO: search for other windows.
  int component = windows_.direction == LEFT_RIGHT ? HTRIGHT : HTBOTTOM;
  window_resizer_.reset(WorkspaceWindowResizer::Create(
      windows_.window1, parent_location, component, grid_size_, windows));
}

void MultiWindowResizeController::Resize(const gfx::Point& screen_location) {
  gfx::Point parent_location(screen_location);
  aura::Window::ConvertPointToWindow(windows_.window1->GetRootWindow(),
                                     windows_.window1->parent(),
                                     &parent_location);
  window_resizer_->Drag(parent_location);
  gfx::Rect bounds = CalculateResizeWidgetBounds();
  if (windows_.direction == LEFT_RIGHT)
    bounds.set_y(show_bounds_.y());
  else
    bounds.set_x(show_bounds_.x());
  resize_widget_->SetBounds(bounds);
}

void MultiWindowResizeController::CompleteResize() {
  window_resizer_->CompleteDrag();
  window_resizer_.reset();

  // Mouse may still be over resizer, if not hide.
  gfx::Point screen_loc = gfx::Screen::GetCursorScreenPoint();
  if (!resize_widget_->GetWindowScreenBounds().Contains(screen_loc))
    Hide();
}

void MultiWindowResizeController::CancelResize() {
  window_resizer_->RevertDrag();
  window_resizer_.reset();
  Hide();
}

gfx::Rect MultiWindowResizeController::CalculateResizeWidgetBounds() const {
  gfx::Size pref = resize_widget_->GetContentsView()->GetPreferredSize();
  int x = 0, y = 0;
  if (windows_.direction == LEFT_RIGHT) {
    x = windows_.window1->bounds().right() - pref.width() / 2;
    y = std::min(windows_.window1->bounds().bottom(),
                 windows_.window2->bounds().bottom()) - kResizeWidgetPadding;
    y -= pref.height() / 2;
  } else {
    x = std::min(windows_.window1->bounds().right(),
                 windows_.window2->bounds().right()) - kResizeWidgetPadding;
    x -= pref.width() / 2;
    y = windows_.window1->bounds().bottom() - pref.height() / 2;
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}

bool MultiWindowResizeController::IsOverWindows(
    const gfx::Point& screen_location) const {
  if (window_resizer_.get())
    return true;  // Ignore hides while actively resizing.

  if (resize_widget_->GetWindowScreenBounds().Contains(screen_location))
    return true;

  return IsOverWindow(windows_.window1, screen_location) ||
      IsOverWindow(windows_.window2, screen_location);
}

bool MultiWindowResizeController::IsOverWindow(
    aura::Window* window,
    const gfx::Point& screen_location) const {
  if (!window->GetScreenBounds().Contains(screen_location))
    return false;

  gfx::Point window_loc(screen_location);
  aura::Window::ConvertPointToWindow(
      window->GetRootWindow(), window->parent(), &window_loc);
  int component = window->delegate()->GetNonClientComponent(window_loc);
  // TODO: this needs to make sure no other window is obscuring window.
  return windows_.Equals(DetermineWindows(window, component, window_loc));
}

}  // namespace internal
}  // namespace ash
