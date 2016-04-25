// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/multi_window_resize_controller.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "grit/ash_resources.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event_targeter.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/coordinate_conversion.h"

using aura::Window;

namespace ash {
namespace {

// Delay before showing.
const int kShowDelayMS = 400;

// Delay before hiding.
const int kHideDelayMS = 500;

// Padding from the bottom/right edge the resize widget is shown at.
const int kResizeWidgetPadding = 15;

bool ContainsX(Window* window, int x) {
  return x >= 0 && x <= window->bounds().width();
}

bool ContainsScreenX(Window* window, int x_in_screen) {
  gfx::Point window_loc(x_in_screen, 0);
  ::wm::ConvertPointFromScreen(window, &window_loc);
  return ContainsX(window, window_loc.x());
}

bool ContainsY(Window* window, int y) {
  return y >= 0 && y <= window->bounds().height();
}

bool ContainsScreenY(Window* window, int y_in_screen) {
  gfx::Point window_loc(0, y_in_screen);
  ::wm::ConvertPointFromScreen(window, &window_loc);
  return ContainsY(window, window_loc.y());
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
    image_ = rb.GetImageNamed(image_id).ToImageSkia();
  }

  // views::View overrides:
  gfx::Size GetPreferredSize() const override {
    return gfx::Size(image_->width(), image_->height());
  }
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->DrawImageInt(*image_, 0, 0);
  }
  bool OnMousePressed(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->StartResize(location);
    return true;
  }
  bool OnMouseDragged(const ui::MouseEvent& event) override {
    gfx::Point location(event.location());
    views::View::ConvertPointToScreen(this, &location);
    controller_->Resize(location, event.flags());
    return true;
  }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    controller_->CompleteResize();
  }
  void OnMouseCaptureLost() override { controller_->CancelResize(); }
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override {
    int component = (direction_ == LEFT_RIGHT) ? HTRIGHT : HTBOTTOM;
    return ::wm::CompoundEventFilter::CursorForWindowComponent(
        component);
  }

 private:
  MultiWindowResizeController* controller_;
  const Direction direction_;
  const gfx::ImageSkia* image_;

  DISALLOW_COPY_AND_ASSIGN(ResizeView);
};

// MouseWatcherHost implementation for MultiWindowResizeController. Forwards
// Contains() to MultiWindowResizeController.
class MultiWindowResizeController::ResizeMouseWatcherHost :
   public views::MouseWatcherHost {
 public:
  ResizeMouseWatcherHost(MultiWindowResizeController* host) : host_(host) {}

  // MouseWatcherHost overrides:
  bool Contains(const gfx::Point& point_in_screen,
                MouseEventType type) override {
    return (type == MOUSE_PRESS)
        ? host_->IsOverResizeWidget(point_in_screen)
        : host_->IsOverWindows(point_in_screen);
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

MultiWindowResizeController::ResizeWindows::ResizeWindows(
    const ResizeWindows& other) = default;

MultiWindowResizeController::ResizeWindows::~ResizeWindows() {
}

bool MultiWindowResizeController::ResizeWindows::Equals(
    const ResizeWindows& other) const {
  return window1 == other.window1 &&
      window2 == other.window2 &&
      direction == other.direction;
}

MultiWindowResizeController::MultiWindowResizeController() {
}

MultiWindowResizeController::~MultiWindowResizeController() {
  window_resizer_.reset();
  Hide();
}

void MultiWindowResizeController::Show(Window* window,
                                       int component,
                                       const gfx::Point& point_in_window) {
  // When the resize widget is showing we ignore Show() requests. Instead we
  // only care about mouse movements from MouseWatcher. This is necessary as
  // WorkspaceEventHandler only sees mouse movements over the windows, not all
  // windows or over the desktop.
  if (resize_widget_)
    return;

  ResizeWindows windows(DetermineWindows(window, component, point_in_window));
  if (IsShowing() && windows_.Equals(windows))
    return;

  Hide();
  if (!windows.is_valid()) {
    windows_ = ResizeWindows();
    return;
  }

  windows_ = windows;
  windows_.window1->AddObserver(this);
  windows_.window2->AddObserver(this);
  show_location_in_parent_ = point_in_window;
  Window::ConvertPointToTarget(
      window, window->parent(), &show_location_in_parent_);
  show_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kShowDelayMS),
      this, &MultiWindowResizeController::ShowIfValidMouseLocation);
}

void MultiWindowResizeController::Hide() {
  if (window_resizer_)
    return;  // Ignore hides while actively resizing.

  if (windows_.window1) {
    windows_.window1->RemoveObserver(this);
    windows_.window1 = NULL;
  }
  if (windows_.window2) {
    windows_.window2->RemoveObserver(this);
    windows_.window2 = NULL;
  }

  show_timer_.Stop();

  if (!resize_widget_)
    return;

  for (size_t i = 0; i < windows_.other_windows.size(); ++i)
    windows_.other_windows[i]->RemoveObserver(this);
  mouse_watcher_.reset();
  resize_widget_.reset();
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
MultiWindowResizeController::DetermineWindowsFromScreenPoint(
    aura::Window* window) const {
  gfx::Point mouse_location(gfx::Screen::GetScreen()->GetCursorScreenPoint());
  ::wm::ConvertPointFromScreen(window, &mouse_location);
  const int component =
      window->delegate()->GetNonClientComponent(mouse_location);
  return DetermineWindows(window, component, mouse_location);
}

void MultiWindowResizeController::CreateMouseWatcher() {
  mouse_watcher_.reset(new views::MouseWatcher(
      new ResizeMouseWatcherHost(this), this));
  mouse_watcher_->set_notify_on_exit_time(
      base::TimeDelta::FromMilliseconds(kHideDelayMS));
  mouse_watcher_->Start();
}

MultiWindowResizeController::ResizeWindows
MultiWindowResizeController::DetermineWindows(
    Window* window,
    int window_component,
    const gfx::Point& point) const {
  ResizeWindows result;
  gfx::Point point_in_parent(point);
  Window::ConvertPointToTarget(window, window->parent(), &point_in_parent);
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
    int x_in_parent,
    int y_in_parent) const {
  Window* parent = window_to_ignore->parent();
  const Window::Windows& windows(parent->children());
  for (Window::Windows::const_reverse_iterator i = windows.rbegin();
       i != windows.rend(); ++i) {
    Window* window = *i;
    if (window == window_to_ignore || !window->IsVisible())
      continue;

    // Ignore windows without a delegate. A delegate is necessary to query the
    // non-client component.
    if (!window->delegate())
      continue;

    gfx::Point p(x_in_parent, y_in_parent);
    aura::Window::ConvertPointToTarget(parent, window, &p);
    switch (edge_want) {
      case HTLEFT:
        if (ContainsY(window, p.y()) && p.x() == 0)
          return window;
        break;
      case HTRIGHT:
        if (ContainsY(window, p.y()) && p.x() == window->bounds().width())
          return window;
        break;
      case HTTOP:
        if (ContainsX(window, p.x()) && p.y() == 0)
          return window;
        break;
      case HTBOTTOM:
        if (ContainsX(window, p.x()) && p.y() == window->bounds().height())
          return window;
        break;
      default:
        NOTREACHED();
    }
    // Window doesn't contain the edge, but if window contains |point|
    // it's obscuring any other window that could be at the location.
    if (window->bounds().Contains(x_in_parent, y_in_parent))
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

void MultiWindowResizeController::ShowIfValidMouseLocation() {
  if (DetermineWindowsFromScreenPoint(windows_.window1).Equals(windows_) ||
      DetermineWindowsFromScreenPoint(windows_.window2).Equals(windows_)) {
    ShowNow();
  } else {
    Hide();
  }
}

void MultiWindowResizeController::ShowNow() {
  DCHECK(!resize_widget_.get());
  DCHECK(windows_.is_valid());
  show_timer_.Stop();
  resize_widget_.reset(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = Shell::GetContainer(Shell::GetTargetRootWindow(),
                                      kShellWindowId_AlwaysOnTopContainer);
  ResizeView* view = new ResizeView(this, windows_.direction);
  resize_widget_->set_focus_on_creation(false);
  resize_widget_->Init(params);
  ::wm::SetWindowVisibilityAnimationType(
      resize_widget_->GetNativeWindow(),
      ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  resize_widget_->GetNativeWindow()->SetName("MultiWindowResizeController");
  resize_widget_->SetContentsView(view);
  show_bounds_in_screen_ = ScreenUtil::ConvertRectToScreen(
      windows_.window1->parent(),
      CalculateResizeWidgetBounds(show_location_in_parent_));
  resize_widget_->SetBounds(show_bounds_in_screen_);
  resize_widget_->Show();
  CreateMouseWatcher();
}

bool MultiWindowResizeController::IsShowing() const {
  return resize_widget_.get() || show_timer_.IsRunning();
}

void MultiWindowResizeController::StartResize(
    const gfx::Point& location_in_screen) {
  DCHECK(!window_resizer_.get());
  DCHECK(windows_.is_valid());
  gfx::Point location_in_parent(location_in_screen);
  aura::client::GetScreenPositionClient(windows_.window2->GetRootWindow())->
      ConvertPointFromScreen(windows_.window2->parent(), &location_in_parent);
  std::vector<aura::Window*> windows;
  windows.push_back(windows_.window2);
  DCHECK(windows_.other_windows.empty());
  FindWindowsTouching(windows_.window2, windows_.direction,
                      &windows_.other_windows);
  for (size_t i = 0; i < windows_.other_windows.size(); ++i) {
    windows_.other_windows[i]->AddObserver(this);
    windows.push_back(windows_.other_windows[i]);
  }
  int component = windows_.direction == LEFT_RIGHT ? HTRIGHT : HTBOTTOM;
  wm::WindowState* window_state = wm::GetWindowState(windows_.window1);
  window_state->CreateDragDetails(location_in_parent, component,
                                  aura::client::WINDOW_MOVE_SOURCE_MOUSE);
  window_resizer_.reset(WorkspaceWindowResizer::Create(
      window_state, wm::WmWindowAura::FromAuraWindows(windows)));

  // Do not hide the resize widget while a drag is active.
  mouse_watcher_.reset();
}

void MultiWindowResizeController::Resize(const gfx::Point& location_in_screen,
                                         int event_flags) {
  gfx::Point location_in_parent(location_in_screen);
  aura::client::GetScreenPositionClient(windows_.window1->GetRootWindow())->
      ConvertPointFromScreen(windows_.window1->parent(), &location_in_parent);
  window_resizer_->Drag(location_in_parent, event_flags);
  gfx::Rect bounds = ScreenUtil::ConvertRectToScreen(
      windows_.window1->parent(),
      CalculateResizeWidgetBounds(location_in_parent));

  if (windows_.direction == LEFT_RIGHT)
    bounds.set_y(show_bounds_in_screen_.y());
  else
    bounds.set_x(show_bounds_in_screen_.x());
  resize_widget_->SetBounds(bounds);
}

void MultiWindowResizeController::CompleteResize() {
  window_resizer_->CompleteDrag();
  window_resizer_->GetTarget()->GetWindowState()->DeleteDragDetails();
  window_resizer_.reset();

  // Mouse may still be over resizer, if not hide.
  gfx::Point screen_loc = gfx::Screen::GetScreen()->GetCursorScreenPoint();
  if (!resize_widget_->GetWindowBoundsInScreen().Contains(screen_loc)) {
    Hide();
  } else {
    // If the mouse is over the resizer we need to remove observers on any of
    // the |other_windows|. If we start another resize we'll recalculate the
    // |other_windows| and invoke AddObserver() as necessary.
    for (size_t i = 0; i < windows_.other_windows.size(); ++i)
      windows_.other_windows[i]->RemoveObserver(this);
    windows_.other_windows.clear();

    CreateMouseWatcher();
  }
}

void MultiWindowResizeController::CancelResize() {
  if (!window_resizer_)
    return;  // Happens if window was destroyed and we nuked the WindowResizer.
  window_resizer_->RevertDrag();
  window_resizer_->GetTarget()->GetWindowState()->DeleteDragDetails();
  window_resizer_.reset();
  Hide();
}

gfx::Rect MultiWindowResizeController::CalculateResizeWidgetBounds(
    const gfx::Point& location_in_parent) const {
  gfx::Size pref = resize_widget_->GetContentsView()->GetPreferredSize();
  int x = 0, y = 0;
  if (windows_.direction == LEFT_RIGHT) {
    x = windows_.window1->bounds().right() - pref.width() / 2;
    y = location_in_parent.y() + kResizeWidgetPadding;
    if (y + pref.height() / 2 > windows_.window1->bounds().bottom() &&
        y + pref.height() / 2 > windows_.window2->bounds().bottom()) {
      y = location_in_parent.y() - kResizeWidgetPadding - pref.height();
    }
  } else {
    x = location_in_parent.x() + kResizeWidgetPadding;
    if (x + pref.height() / 2 > windows_.window1->bounds().right() &&
        x + pref.height() / 2 > windows_.window2->bounds().right()) {
      x = location_in_parent.x() - kResizeWidgetPadding - pref.width();
    }
    y = windows_.window1->bounds().bottom() - pref.height() / 2;
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}

bool MultiWindowResizeController::IsOverResizeWidget(
    const gfx::Point& location_in_screen) const {
  return resize_widget_->GetWindowBoundsInScreen().Contains(
      location_in_screen);
}

bool MultiWindowResizeController::IsOverWindows(
    const gfx::Point& location_in_screen) const {
  if (IsOverResizeWidget(location_in_screen))
    return true;

  if (windows_.direction == TOP_BOTTOM) {
    if (!ContainsScreenX(windows_.window1, location_in_screen.x()) ||
        !ContainsScreenX(windows_.window2, location_in_screen.x())) {
      return false;
    }
  } else {
    if (!ContainsScreenY(windows_.window1, location_in_screen.y()) ||
        !ContainsScreenY(windows_.window2, location_in_screen.y())) {
      return false;
    }
  }

  // Check whether |location_in_screen| is in the event target's resize region.
  // This is tricky because a window's resize region can extend outside a
  // window's bounds.
  gfx::Point location_in_root(location_in_screen);
  aura::Window* root = windows_.window1->GetRootWindow();
  ::wm::ConvertPointFromScreen(root, &location_in_root);
  ui::MouseEvent test_event(ui::ET_MOUSE_MOVED, location_in_root,
                            location_in_root, ui::EventTimeForNow(),
                            ui::EF_NONE, ui::EF_NONE);
  ui::EventTarget* event_handler = static_cast<ui::EventTarget*>(root)
                                       ->GetEventTargeter()
                                       ->FindTargetForEvent(root, &test_event);
  if (event_handler == windows_.window1) {
    return IsOverComponent(
        windows_.window1,
        location_in_screen,
        windows_.direction == TOP_BOTTOM ? HTBOTTOM : HTRIGHT);
  } else if (event_handler == windows_.window2) {
    return IsOverComponent(
        windows_.window2,
        location_in_screen,
        windows_.direction == TOP_BOTTOM ? HTTOP : HTLEFT);
  }
  return false;
}

bool MultiWindowResizeController::IsOverComponent(
    aura::Window* window,
    const gfx::Point& location_in_screen,
    int component) const {
  gfx::Point window_loc(location_in_screen);
  ::wm::ConvertPointFromScreen(window, &window_loc);
  return window->delegate()->GetNonClientComponent(window_loc) == component;
}

}  // namespace ash
