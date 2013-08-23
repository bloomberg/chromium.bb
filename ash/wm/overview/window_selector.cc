// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector.h"

#include <algorithm>

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/window_selector_delegate.h"
#include "ash/wm/overview/window_selector_window.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const float kCardAspectRatio = 4.0f / 3.0f;
const int kWindowMargin = 30;
const int kMinCardsMajor = 3;
const int kOverviewSelectorTransitionMilliseconds = 100;
const SkColor kWindowSelectorSelectionColor = SK_ColorBLACK;
const float kWindowSelectorSelectionOpacity = 0.5f;
const int kWindowSelectorSelectionPadding = 15;

// A comparator for locating a given target window.
struct WindowSelectorWindowComparator
    : public std::unary_function<WindowSelectorWindow*, bool> {
  explicit WindowSelectorWindowComparator(const aura::Window* target_window)
      : target(target_window) {
  }

  bool operator()(const WindowSelectorWindow* window) const {
    return target == window->window();
  }

  const aura::Window* target;
};

}  // namespace

WindowSelector::WindowSelector(const WindowList& windows,
                               WindowSelector::Mode mode,
                               WindowSelectorDelegate* delegate)
    : mode_(mode),
      delegate_(delegate),
      selected_window_(0),
      selection_root_(NULL) {
  DCHECK(delegate_);
  for (size_t i = 0; i < windows.size(); ++i) {
    windows[i]->AddObserver(this);
    windows_.push_back(new WindowSelectorWindow(windows[i]));
  }
  if (mode == WindowSelector::CYCLE)
    selection_root_ = ash::Shell::GetActiveRootWindow();
  PositionWindows();
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

WindowSelector::~WindowSelector() {
  for (size_t i = 0; i < windows_.size(); i++) {
    windows_[i]->window()->RemoveObserver(this);
  }
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void WindowSelector::Step(WindowSelector::Direction direction) {
  DCHECK(windows_.size() > 0);
  if (!selection_widget_)
    InitializeSelectionWidget();
  selected_window_ = (selected_window_ + windows_.size() +
      (direction == WindowSelector::FORWARD ? 1 : -1)) % windows_.size();
  UpdateSelectionLocation(true);
}

void WindowSelector::SelectWindow() {
  delegate_->OnWindowSelected(windows_[selected_window_]->window());
}

void WindowSelector::OnEvent(ui::Event* event) {
  // If the event is targetted at any of the windows in the overview, then
  // prevent it from propagating.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i]->Contains(target)) {
      // TODO(flackr): StopPropogation prevents generation of gesture events.
      // We should find a better way to prevent events from being delivered to
      // the window, perhaps a transparent window in front of the target window
      // or using EventClientImpl::CanProcessEventsWithinSubtree.
      event->StopPropagation();
      break;
    }
  }

  // This object may not be valid after this call as a selection event can
  // trigger deletion of the window selector.
  ui::EventHandler::OnEvent(event);
}

void WindowSelector::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_RELEASED)
    return;
  WindowSelectorWindow* target = GetEventTarget(event);
  if (!target)
    return;

  HandleSelectionEvent(target);
}

void WindowSelector::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() != ui::ET_TOUCH_PRESSED)
    return;
  WindowSelectorWindow* target = GetEventTarget(event);
  if (!target)
    return;

  HandleSelectionEvent(target);
}

void WindowSelector::OnWindowDestroyed(aura::Window* window) {
  ScopedVector<WindowSelectorWindow>::iterator iter =
      std::find_if(windows_.begin(), windows_.end(),
                   WindowSelectorWindowComparator(window));
  DCHECK(iter != windows_.end());
  size_t deleted_index = iter - windows_.begin();
  (*iter)->OnWindowDestroyed();
  windows_.erase(iter);
  if (windows_.empty()) {
    delegate_->OnSelectionCanceled();
    return;
  }
  if (selected_window_ >= deleted_index) {
    if (selected_window_ > deleted_index)
      selected_window_--;
    selected_window_ = selected_window_ % windows_.size();
    UpdateSelectionLocation(true);
  }

  PositionWindows();
}

WindowSelectorWindow* WindowSelector::GetEventTarget(ui::LocatedEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  // If the target window doesn't actually contain the event location (i.e.
  // mouse down over the window and mouse up elsewhere) then do not select the
  // window.
  if (!target->HitTest(event->location()))
    return NULL;

  for (size_t i = 0; i < windows_.size(); i++) {
    if (windows_[i]->Contains(target))
      return windows_[i];
  }
  return NULL;
}

void WindowSelector::HandleSelectionEvent(WindowSelectorWindow* target) {
  // The selected window should not be minimized when window selection is
  // ended.
  target->RestoreWindowOnExit();
  delegate_->OnWindowSelected(target->window());
}

void WindowSelector::PositionWindows() {
  if (selection_root_) {
    DCHECK_EQ(mode_, CYCLE);
    std::vector<WindowSelectorWindow*> windows;
    for (size_t i = 0; i < windows_.size(); ++i)
      windows.push_back(windows_[i]);
    PositionWindowsOnRoot(selection_root_, windows);
  } else {
    DCHECK_EQ(mode_, OVERVIEW);
    Shell::RootWindowList root_window_list = Shell::GetAllRootWindows();
    for (size_t i = 0; i < root_window_list.size(); ++i)
      PositionWindowsFromRoot(root_window_list[i]);
  }
}

void WindowSelector::PositionWindowsFromRoot(aura::RootWindow* root_window) {
  std::vector<WindowSelectorWindow*> windows;
  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i]->window()->GetRootWindow() == root_window)
      windows.push_back(windows_[i]);
  }
  PositionWindowsOnRoot(root_window, windows);
}

void WindowSelector::PositionWindowsOnRoot(
    aura::RootWindow* root_window,
    const std::vector<WindowSelectorWindow*>& windows) {
  if (windows.empty())
    return;

  gfx::Size window_size;
  gfx::Rect total_bounds = ScreenAsh::ConvertRectToScreen(root_window,
      ScreenAsh::GetDisplayWorkAreaBoundsInParent(
      Shell::GetContainer(root_window,
                          internal::kShellWindowId_DefaultContainer)));

  // Find the minimum number of windows per row that will fit all of the
  // windows on screen.
  size_t columns = std::max(
      total_bounds.width() > total_bounds.height() ? kMinCardsMajor : 1,
      static_cast<int>(ceil(sqrt(total_bounds.width() * windows.size() /
                                 (kCardAspectRatio * total_bounds.height())))));
  size_t rows = ((windows.size() + columns - 1) / columns);
  window_size.set_width(std::min(
      static_cast<int>(total_bounds.width() / columns),
      static_cast<int>(total_bounds.height() * kCardAspectRatio / rows)));
  window_size.set_height(window_size.width() / kCardAspectRatio);

  // Calculate the X and Y offsets necessary to center the grid.
  int x_offset = total_bounds.x() + ((windows.size() >= columns ? 0 :
      (columns - windows.size()) * window_size.width()) +
      (total_bounds.width() - columns * window_size.width())) / 2;
  int y_offset = total_bounds.y() + (total_bounds.height() -
      rows * window_size.height()) / 2;
  for (size_t i = 0; i < windows.size(); ++i) {
    gfx::Transform transform;
    int column = i % columns;
    int row = i / columns;
    gfx::Rect target_bounds(window_size.width() * column + x_offset,
                            window_size.height() * row + y_offset,
                            window_size.width(),
                            window_size.height());
    target_bounds.Inset(kWindowMargin, kWindowMargin);
    windows[i]->TransformToFitBounds(root_window, target_bounds);
  }
}

void WindowSelector::InitializeSelectionWidget() {
  selection_widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.can_activate = false;
  params.keep_on_top = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
  params.parent = Shell::GetContainer(
      selection_root_,
      internal::kShellWindowId_DefaultContainer);
  params.accept_events = false;
  selection_widget_->set_focus_on_creation(false);
  selection_widget_->Init(params);
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateSolidBackground(kWindowSelectorSelectionColor));
  selection_widget_->SetContentsView(content_view);
  UpdateSelectionLocation(false);
  selection_widget_->GetNativeWindow()->parent()->StackChildAtBottom(
      selection_widget_->GetNativeWindow());
  selection_widget_->Show();
  selection_widget_->GetNativeWindow()->layer()->SetOpacity(
      kWindowSelectorSelectionOpacity);
}

void WindowSelector::UpdateSelectionLocation(bool animate) {
  if (!selection_widget_)
    return;
  gfx::Rect target_bounds = windows_[selected_window_]->bounds();
  target_bounds.Inset(-kWindowSelectorSelectionPadding,
                      -kWindowSelectorSelectionPadding);
  if (animate) {
    ui::ScopedLayerAnimationSettings animation_settings(
        selection_widget_->GetNativeWindow()->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kOverviewSelectorTransitionMilliseconds));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    selection_widget_->SetBounds(target_bounds);
  } else {
    selection_widget_->SetBounds(target_bounds);
  }
}

}  // namespace ash
