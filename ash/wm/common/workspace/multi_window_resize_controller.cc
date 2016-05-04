// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/workspace/multi_window_resize_controller.h"

#include "ash/wm/common/wm_lookup.h"
#include "ash/wm/common/wm_root_window_controller.h"
#include "ash/wm/common/wm_shell_window_ids.h"
#include "ash/wm/common/wm_window.h"
#include "ash/wm/common/workspace/workspace_window_resizer.h"
#include "grit/ash_resources.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/compound_event_filter.h"

namespace ash {
namespace {

// Delay before showing.
const int kShowDelayMS = 400;

// Delay before hiding.
const int kHideDelayMS = 500;

// Padding from the bottom/right edge the resize widget is shown at.
const int kResizeWidgetPadding = 15;

bool ContainsX(wm::WmWindow* window, int x) {
  return x >= 0 && x <= window->GetBounds().width();
}

bool ContainsScreenX(wm::WmWindow* window, int x_in_screen) {
  gfx::Point window_loc =
      window->ConvertPointFromScreen(gfx::Point(x_in_screen, 0));
  return ContainsX(window, window_loc.x());
}

bool ContainsY(wm::WmWindow* window, int y) {
  return y >= 0 && y <= window->GetBounds().height();
}

bool ContainsScreenY(wm::WmWindow* window, int y_in_screen) {
  gfx::Point window_loc =
      window->ConvertPointFromScreen(gfx::Point(0, y_in_screen));
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
      : controller_(controller), direction_(direction), image_(NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    int image_id = direction == TOP_BOTTOM ? IDR_AURA_MULTI_WINDOW_RESIZE_H
                                           : IDR_AURA_MULTI_WINDOW_RESIZE_V;
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
    return ::wm::CompoundEventFilter::CursorForWindowComponent(component);
  }

 private:
  MultiWindowResizeController* controller_;
  const Direction direction_;
  const gfx::ImageSkia* image_;

  DISALLOW_COPY_AND_ASSIGN(ResizeView);
};

// MouseWatcherHost implementation for MultiWindowResizeController. Forwards
// Contains() to MultiWindowResizeController.
class MultiWindowResizeController::ResizeMouseWatcherHost
    : public views::MouseWatcherHost {
 public:
  ResizeMouseWatcherHost(MultiWindowResizeController* host) : host_(host) {}

  // MouseWatcherHost overrides:
  bool Contains(const gfx::Point& point_in_screen,
                MouseEventType type) override {
    return (type == MOUSE_PRESS) ? host_->IsOverResizeWidget(point_in_screen)
                                 : host_->IsOverWindows(point_in_screen);
  }

 private:
  MultiWindowResizeController* host_;

  DISALLOW_COPY_AND_ASSIGN(ResizeMouseWatcherHost);
};

MultiWindowResizeController::ResizeWindows::ResizeWindows()
    : window1(nullptr), window2(nullptr), direction(TOP_BOTTOM) {}

MultiWindowResizeController::ResizeWindows::ResizeWindows(
    const ResizeWindows& other) = default;

MultiWindowResizeController::ResizeWindows::~ResizeWindows() {}

bool MultiWindowResizeController::ResizeWindows::Equals(
    const ResizeWindows& other) const {
  return window1 == other.window1 && window2 == other.window2 &&
         direction == other.direction;
}

MultiWindowResizeController::MultiWindowResizeController() {}

MultiWindowResizeController::~MultiWindowResizeController() {
  window_resizer_.reset();
  Hide();
}

void MultiWindowResizeController::Show(wm::WmWindow* window,
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
  show_location_in_parent_ =
      window->ConvertPointToTarget(window->GetParent(), point_in_window);
  show_timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(kShowDelayMS),
                    this,
                    &MultiWindowResizeController::ShowIfValidMouseLocation);
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

void MultiWindowResizeController::OnWindowDestroying(wm::WmWindow* window) {
  // Have to explicitly reset the WindowResizer, otherwise Hide() does nothing.
  window_resizer_.reset();
  Hide();
}

MultiWindowResizeController::ResizeWindows
MultiWindowResizeController::DetermineWindowsFromScreenPoint(
    wm::WmWindow* window) const {
  gfx::Point mouse_location(
      display::Screen::GetScreen()->GetCursorScreenPoint());
  mouse_location = window->ConvertPointFromScreen(mouse_location);
  const int component = window->GetNonClientComponent(mouse_location);
  return DetermineWindows(window, component, mouse_location);
}

void MultiWindowResizeController::CreateMouseWatcher() {
  mouse_watcher_.reset(
      new views::MouseWatcher(new ResizeMouseWatcherHost(this), this));
  mouse_watcher_->set_notify_on_exit_time(
      base::TimeDelta::FromMilliseconds(kHideDelayMS));
  mouse_watcher_->Start();
}

MultiWindowResizeController::ResizeWindows
MultiWindowResizeController::DetermineWindows(wm::WmWindow* window,
                                              int window_component,
                                              const gfx::Point& point) const {
  ResizeWindows result;
  gfx::Point point_in_parent =
      window->ConvertPointToTarget(window->GetParent(), point);
  switch (window_component) {
    case HTRIGHT:
      result.direction = LEFT_RIGHT;
      result.window1 = window;
      result.window2 = FindWindowByEdge(
          window, HTLEFT, window->GetBounds().right(), point_in_parent.y());
      break;
    case HTLEFT:
      result.direction = LEFT_RIGHT;
      result.window1 = FindWindowByEdge(
          window, HTRIGHT, window->GetBounds().x(), point_in_parent.y());
      result.window2 = window;
      break;
    case HTTOP:
      result.direction = TOP_BOTTOM;
      result.window1 = FindWindowByEdge(window, HTBOTTOM, point_in_parent.x(),
                                        window->GetBounds().y());
      result.window2 = window;
      break;
    case HTBOTTOM:
      result.direction = TOP_BOTTOM;
      result.window1 = window;
      result.window2 = FindWindowByEdge(window, HTTOP, point_in_parent.x(),
                                        window->GetBounds().bottom());
      break;
    default:
      break;
  }
  return result;
}

wm::WmWindow* MultiWindowResizeController::FindWindowByEdge(
    wm::WmWindow* window_to_ignore,
    int edge_want,
    int x_in_parent,
    int y_in_parent) const {
  wm::WmWindow* parent = window_to_ignore->GetParent();
  std::vector<wm::WmWindow*> windows = parent->GetChildren();
  for (auto i = windows.rbegin(); i != windows.rend(); ++i) {
    wm::WmWindow* window = *i;
    if (window == window_to_ignore || !window->IsVisible())
      continue;

    // Ignore windows without a non-client area.
    if (!window->HasNonClientArea())
      continue;

    gfx::Point p = parent->ConvertPointToTarget(
        window, gfx::Point(x_in_parent, y_in_parent));
    switch (edge_want) {
      case HTLEFT:
        if (ContainsY(window, p.y()) && p.x() == 0)
          return window;
        break;
      case HTRIGHT:
        if (ContainsY(window, p.y()) && p.x() == window->GetBounds().width())
          return window;
        break;
      case HTTOP:
        if (ContainsX(window, p.x()) && p.y() == 0)
          return window;
        break;
      case HTBOTTOM:
        if (ContainsX(window, p.x()) && p.y() == window->GetBounds().height())
          return window;
        break;
      default:
        NOTREACHED();
    }
    // Window doesn't contain the edge, but if window contains |point|
    // it's obscuring any other window that could be at the location.
    if (window->GetBounds().Contains(x_in_parent, y_in_parent))
      return NULL;
  }
  return NULL;
}

wm::WmWindow* MultiWindowResizeController::FindWindowTouching(
    wm::WmWindow* window,
    Direction direction) const {
  int right = window->GetBounds().right();
  int bottom = window->GetBounds().bottom();
  wm::WmWindow* parent = window->GetParent();
  std::vector<wm::WmWindow*> windows = parent->GetChildren();
  for (auto i = windows.rbegin(); i != windows.rend(); ++i) {
    wm::WmWindow* other = *i;
    if (other == window || !other->IsVisible())
      continue;
    switch (direction) {
      case TOP_BOTTOM:
        if (other->GetBounds().y() == bottom &&
            Intersects(other->GetBounds().x(), other->GetBounds().right(),
                       window->GetBounds().x(), window->GetBounds().right())) {
          return other;
        }
        break;
      case LEFT_RIGHT:
        if (other->GetBounds().x() == right &&
            Intersects(other->GetBounds().y(), other->GetBounds().bottom(),
                       window->GetBounds().y(), window->GetBounds().bottom())) {
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
    wm::WmWindow* start,
    Direction direction,
    std::vector<wm::WmWindow*>* others) const {
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
  params.name = "MultiWindowResizeController";
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  windows_.window1->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          resize_widget_.get(), kShellWindowId_AlwaysOnTopContainer, &params);
  ResizeView* view = new ResizeView(this, windows_.direction);
  resize_widget_->set_focus_on_creation(false);
  resize_widget_->Init(params);
  wm::WmLookup::Get()
      ->GetWindowForWidget(resize_widget_.get())
      ->SetVisibilityAnimationType(::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  resize_widget_->SetContentsView(view);
  show_bounds_in_screen_ = windows_.window1->GetParent()->ConvertRectToScreen(
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
  gfx::Point location_in_parent =
      windows_.window2->GetParent()->ConvertPointFromScreen(location_in_screen);
  std::vector<wm::WmWindow*> windows;
  windows.push_back(windows_.window2);
  DCHECK(windows_.other_windows.empty());
  FindWindowsTouching(windows_.window2, windows_.direction,
                      &windows_.other_windows);
  for (size_t i = 0; i < windows_.other_windows.size(); ++i) {
    windows_.other_windows[i]->AddObserver(this);
    windows.push_back(windows_.other_windows[i]);
  }
  int component = windows_.direction == LEFT_RIGHT ? HTRIGHT : HTBOTTOM;
  wm::WindowState* window_state = windows_.window1->GetWindowState();
  window_state->CreateDragDetails(location_in_parent, component,
                                  aura::client::WINDOW_MOVE_SOURCE_MOUSE);
  window_resizer_.reset(WorkspaceWindowResizer::Create(window_state, windows));

  // Do not hide the resize widget while a drag is active.
  mouse_watcher_.reset();
}

void MultiWindowResizeController::Resize(const gfx::Point& location_in_screen,
                                         int event_flags) {
  gfx::Point location_in_parent =
      windows_.window1->GetParent()->ConvertPointFromScreen(location_in_screen);
  window_resizer_->Drag(location_in_parent, event_flags);
  gfx::Rect bounds = windows_.window1->GetParent()->ConvertRectToScreen(
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
  gfx::Point screen_loc = display::Screen::GetScreen()->GetCursorScreenPoint();
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
    x = windows_.window1->GetBounds().right() - pref.width() / 2;
    y = location_in_parent.y() + kResizeWidgetPadding;
    if (y + pref.height() / 2 > windows_.window1->GetBounds().bottom() &&
        y + pref.height() / 2 > windows_.window2->GetBounds().bottom()) {
      y = location_in_parent.y() - kResizeWidgetPadding - pref.height();
    }
  } else {
    x = location_in_parent.x() + kResizeWidgetPadding;
    if (x + pref.height() / 2 > windows_.window1->GetBounds().right() &&
        x + pref.height() / 2 > windows_.window2->GetBounds().right()) {
      x = location_in_parent.x() - kResizeWidgetPadding - pref.width();
    }
    y = windows_.window1->GetBounds().bottom() - pref.height() / 2;
  }
  return gfx::Rect(x, y, pref.width(), pref.height());
}

bool MultiWindowResizeController::IsOverResizeWidget(
    const gfx::Point& location_in_screen) const {
  return resize_widget_->GetWindowBoundsInScreen().Contains(location_in_screen);
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
  wm::WmWindow* target =
      windows_.window1->GetRootWindowController()->FindEventTarget(
          location_in_screen);
  if (target == windows_.window1) {
    return IsOverComponent(
        windows_.window1, location_in_screen,
        windows_.direction == TOP_BOTTOM ? HTBOTTOM : HTRIGHT);
  }
  if (target == windows_.window2) {
    return IsOverComponent(windows_.window2, location_in_screen,
                           windows_.direction == TOP_BOTTOM ? HTTOP : HTLEFT);
  }
  return false;
}

bool MultiWindowResizeController::IsOverComponent(
    wm::WmWindow* window,
    const gfx::Point& location_in_screen,
    int component) const {
  gfx::Point window_loc = window->ConvertPointFromScreen(location_in_screen);
  return window->GetNonClientComponent(window_loc) == component;
}

}  // namespace ash
