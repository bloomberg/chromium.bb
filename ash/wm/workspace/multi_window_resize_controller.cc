// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/multi_window_resize_controller.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/workspace/workspace_event_filter.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "grit/ui_resources.h"
#include "ui/aura/event_filter.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/root_window_event_filter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using aura::Window;

namespace ash {
namespace internal {

namespace {

// Delay before showing.
const int kShowDelayMS = 100;

// Delay before hiding.
const int kHideDelayMS = 500;

// Padding from the bottom/right edge the resize widget is shown at.
const int kResizeWidgetPadding = 15;

bool ContainsX(Window* window, int x) {
  return window->bounds().x() <= x && window->bounds().right() >= x;
}

bool ContainsY(Window* window, int y) {
  return window->bounds().y() <= y && window->bounds().bottom() >= y;
}

bool Intersects(int x1, int max_1, int x2, int max_2) {
  return x2 <= max_1 && max_2 > x1;
}

}  // namespace

// View contained in the widget. Passes along mouse events to the
// MultiWindowResizeController so that it can start/stop the resize loop.
class MultiWindowResizeController::ResizeView : public views::View {
 public:
  explicit ResizeView(MultiWindowResizeController* controller,
                      Direction direction)
      : controller_(controller),
        direction_(direction),
        image_(NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    int image_id =
        direction == TOP_BOTTOM ? IDR_AURA_MULTI_WINDOW_RESIZE_H :
                                  IDR_AURA_MULTI_WINDOW_RESIZE_V;
    image_ = rb.GetImageNamed(image_id).ToSkBitmap();
  }

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(image_->width(), image_->height());
  }
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawBitmapInt(*image_, 0, 0);
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
    controller_->Resize(location, event.flags());
    return true;
  }
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE {
    controller_->CompleteResize(event.flags());
  }
  virtual void OnMouseCaptureLost() OVERRIDE {
    controller_->CancelResize();
  }
  virtual gfx::NativeCursor GetCursor(
      const views::MouseEvent& event) OVERRIDE {
    int component = (direction_ == LEFT_RIGHT) ? HTRIGHT : HTBOTTOM;
    return aura::shared::RootWindowEventFilter::CursorForWindowComponent(
        component);
  }

 private:
  MultiWindowResizeController* controller_;
  const Direction direction_;
  const SkBitmap* image_;

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
    return host_->IsOverWindows(screen_point);
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
  // When the resize widget is showing we ignore Show() requests. Instead we
  // only care about mouse movements from MouseWatcher. This is necessary as
  // WorkspaceEventFilter only sees mouse movements over the windows, not all
  // windows or over the desktop.
  if (resize_widget_)
    return;

  ResizeWindows windows(DetermineWindows(window, component, point));
  if (IsShowing()) {
    if (windows_.Equals(windows))
      return;  // Over the same windows.
    DelayedHide();
  }

  if (!windows.is_valid())
    return;
  Hide();
  windows_ = windows;
  windows_.window1->AddObserver(this);
  windows_.window2->AddObserver(this);
  show_location_ = point;
  Window::ConvertPointToWindow(window, window->parent(), &show_location_);
  if (show_timer_.IsRunning())
    return;
  show_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kShowDelayMS),
                    this, &MultiWindowResizeController::ShowNow);
}

void MultiWindowResizeController::Hide() {
  hide_timer_.Stop();
  if (window_resizer_.get())
    return;  // Ignore hides while actively resizing.

  show_timer_.Stop();
  if (!resize_widget_)
    return;
  windows_.window1->RemoveObserver(this);
  windows_.window2->RemoveObserver(this);
  for (size_t i = 0; i < windows_.other_windows.size(); ++i)
    windows_.other_windows[i]->RemoveObserver(this);
  mouse_watcher_.reset();
  resize_widget_->Close();
  resize_widget_ = NULL;
  windows_ = ResizeWindows();
}

void MultiWindowResizeController::MouseMovedOutOfHost() {
  Hide();
}

void MultiWindowResizeController::OnWindowDestroying(
    aura::Window* window) {
  // Have to explicitly reset the WindowResizer, otherwise Hide() does nothing.
  window_resizer_.reset();
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

aura::Window* MultiWindowResizeController::FindWindowTouching(
    aura::Window* window,
    Direction direction) const {
  int right = window->bounds().right();
  int bottom = window->bounds().bottom();
  Window* parent = window->parent();
  const Window::Windows& windows(parent->children());
  for (Window::Windows::const_reverse_iterator i = windows.rbegin();
       i != windows.rend(); ++i) {
    Window* other = *i;
    if (other == window || !other->IsVisible())
      continue;
    switch (direction) {
      case TOP_BOTTOM:
        if (other->bounds().y() == bottom &&
            Intersects(other->bounds().x(), other->bounds().right(),
                       window->bounds().x(), window->bounds().right())) {
          return other;
        }
        break;
      case LEFT_RIGHT:
        if (other->bounds().x() == right &&
            Intersects(other->bounds().y(), other->bounds().bottom(),
                       window->bounds().y(), window->bounds().bottom())) {
          return other;
        }
        break;
      default:
        NOTREACHED();
    }
  }
  return NULL;
}

void MultiWindowResizeController::FindWindowsTouching(
    aura::Window* start,
    Direction direction,
    std::vector<aura::Window*>* others) const {
  while (start) {
    start = FindWindowTouching(start, direction);
    if (start)
      others->push_back(start);
  }
}

void MultiWindowResizeController::DelayedHide() {
  if (hide_timer_.IsRunning())
    return;

  hide_timer_.Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kHideDelayMS),
                    this, &MultiWindowResizeController::Hide);
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
  SetWindowVisibilityAnimationType(
      resize_widget_->GetNativeWindow(),
      WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  resize_widget_->GetNativeWindow()->SetName("MultiWindowResizeController");
  resize_widget_->SetContentsView(view);
  show_bounds_ = CalculateResizeWidgetBounds(show_location_);
  resize_widget_->SetBounds(show_bounds_);
  resize_widget_->Show();
  mouse_watcher_.reset(new views::MouseWatcher(
                           new ResizeMouseWatcherHost(this),
                           this));
  mouse_watcher_->set_notify_on_exit_time(
      base::TimeDelta::FromMilliseconds(kHideDelayMS));
  mouse_watcher_->Start();
}

bool MultiWindowResizeController::IsShowing() const {
  return resize_widget_ || show_timer_.IsRunning();
}

void MultiWindowResizeController::StartResize(
    const gfx::Point& screen_location) {
  DCHECK(!window_resizer_.get());
  DCHECK(windows_.is_valid());
  hide_timer_.Stop();
  gfx::Point parent_location(screen_location);
  aura::Window::ConvertPointToWindow(
      windows_.window1->GetRootWindow(), windows_.window1->parent(),
      &parent_location);
  std::vector<aura::Window*> windows;
  windows.push_back(windows_.window2);
  FindWindowsTouching(windows_.window2, windows_.direction,
                      &windows_.other_windows);
  for (size_t i = 0; i < windows_.other_windows.size(); ++i) {
    windows_.other_windows[i]->AddObserver(this);
    windows.push_back(windows_.other_windows[i]);
  }
  int component = windows_.direction == LEFT_RIGHT ? HTRIGHT : HTBOTTOM;
  window_resizer_.reset(WorkspaceWindowResizer::Create(
      windows_.window1, parent_location, component, windows));
}

void MultiWindowResizeController::Resize(const gfx::Point& screen_location,
                                         int event_flags) {
  gfx::Point parent_location(screen_location);
  aura::Window::ConvertPointToWindow(windows_.window1->GetRootWindow(),
                                     windows_.window1->parent(),
                                     &parent_location);
  window_resizer_->Drag(parent_location, event_flags);
  gfx::Rect bounds = CalculateResizeWidgetBounds(parent_location);
  if (windows_.direction == LEFT_RIGHT)
    bounds.set_y(show_bounds_.y());
  else
    bounds.set_x(show_bounds_.x());
  resize_widget_->SetBounds(bounds);
}

void MultiWindowResizeController::CompleteResize(int event_flags) {
  window_resizer_->CompleteDrag(event_flags);
  window_resizer_.reset();

  // Mouse may still be over resizer, if not hide.
  gfx::Point screen_loc = gfx::Screen::GetCursorScreenPoint();
  if (!resize_widget_->GetWindowScreenBounds().Contains(screen_loc))
    Hide();
}

void MultiWindowResizeController::CancelResize() {
  if (!window_resizer_.get())
    return;  // Happens if window was destroyed and we nuked the WindowResizer.
  window_resizer_->RevertDrag();
  window_resizer_.reset();
  Hide();
}

gfx::Rect MultiWindowResizeController::CalculateResizeWidgetBounds(
    const gfx::Point& location) const {
  gfx::Size pref = resize_widget_->GetContentsView()->GetPreferredSize();
  int x = 0, y = 0;
  if (windows_.direction == LEFT_RIGHT) {
    x = windows_.window1->bounds().right() - pref.width() / 2;
    y = location.y() + kResizeWidgetPadding;
    if (y + pref.height() / 2 > windows_.window1->bounds().bottom() &&
        y + pref.height() / 2 > windows_.window2->bounds().bottom()) {
      y = location.y() - kResizeWidgetPadding - pref.height();
    }
  } else {
    x = location.x() + kResizeWidgetPadding;
    if (x + pref.height() / 2 > windows_.window1->bounds().right() &&
        x + pref.height() / 2 > windows_.window2->bounds().right()) {
      x = location.x() - kResizeWidgetPadding - pref.width();
    }
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

  int hit1, hit2;
  if (windows_.direction == TOP_BOTTOM) {
    hit1 = HTBOTTOM;
    hit2 = HTTOP;
  } else {
    hit1 = HTRIGHT;
    hit2 = HTLEFT;
  }

  return IsOverWindow(windows_.window1, screen_location, hit1) ||
      IsOverWindow(windows_.window2, screen_location, hit2);
}

bool MultiWindowResizeController::IsOverWindow(
    aura::Window* window,
    const gfx::Point& screen_location,
    int component) const {
  if (!window->delegate())
    return false;

  gfx::Point window_loc(screen_location);
  aura::Window::ConvertPointToWindow(
      window->GetRootWindow(), window, &window_loc);
  return window->HitTest(window_loc) &&
      window->delegate()->GetNonClientComponent(window_loc) == component;
}

}  // namespace internal
}  // namespace ash
