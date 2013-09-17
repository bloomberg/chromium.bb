// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_overview.h"

#include <algorithm>

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_window.h"
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
const SkColor kWindowOverviewSelectionColor = SK_ColorBLACK;
const float kWindowOverviewSelectionOpacity = 0.5f;
const int kWindowOverviewSelectionPadding = 15;

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

WindowOverview::WindowOverview(WindowSelector* window_selector,
                               WindowSelectorWindowList* windows,
                               aura::RootWindow* single_root_window)
    : window_selector_(window_selector),
      windows_(windows),
      single_root_window_(single_root_window) {
  PositionWindows();
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

WindowOverview::~WindowOverview() {
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void WindowOverview::SetSelection(size_t index) {
  DCHECK_LT(index, windows_->size());
  gfx::Rect target_bounds = (*windows_)[index]->bounds();
  target_bounds.Inset(-kWindowOverviewSelectionPadding,
                      -kWindowOverviewSelectionPadding);

  if (selection_widget_) {
    // If the selection widget is already active, animate to the new bounds.
    ui::ScopedLayerAnimationSettings animation_settings(
        selection_widget_->GetNativeWindow()->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kOverviewSelectorTransitionMilliseconds));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    selection_widget_->SetBounds(target_bounds);
  } else {
    InitializeSelectionWidget();
    selection_widget_->SetBounds(target_bounds);
  }
}

void WindowOverview::OnWindowsChanged() {
  PositionWindows();
}

void WindowOverview::OnEvent(ui::Event* event) {
  // If the event is targetted at any of the windows in the overview, then
  // prevent it from propagating.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  for (WindowSelectorWindowList::iterator iter = windows_->begin();
       iter != windows_->end(); ++iter) {
    if ((*iter)->Contains(target)) {
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

void WindowOverview::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() != ui::ET_KEY_PRESSED)
    return;
  if (event->key_code() == ui::VKEY_ESCAPE)
    window_selector_->CancelSelection();
}

void WindowOverview::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_RELEASED)
    return;
  WindowSelectorWindow* target = GetEventTarget(event);
  if (!target)
    return;

  window_selector_->SelectWindow(target->window());
}

void WindowOverview::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() != ui::ET_TOUCH_PRESSED)
    return;
  WindowSelectorWindow* target = GetEventTarget(event);
  if (!target)
    return;

  window_selector_->SelectWindow(target->window());
}

WindowSelectorWindow* WindowOverview::GetEventTarget(ui::LocatedEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  // If the target window doesn't actually contain the event location (i.e.
  // mouse down over the window and mouse up elsewhere) then do not select the
  // window.
  if (!target->HitTest(event->location()))
    return NULL;

  for (WindowSelectorWindowList::iterator iter = windows_->begin();
       iter != windows_->end(); ++iter) {
    if ((*iter)->Contains(target))
      return *iter;
  }
  return NULL;
}

void WindowOverview::PositionWindows() {
  if (single_root_window_) {
    std::vector<WindowSelectorWindow*> windows;
    for (WindowSelectorWindowList::iterator iter = windows_->begin();
         iter != windows_->end(); ++iter) {
      windows.push_back(*iter);
    }
    PositionWindowsOnRoot(single_root_window_, windows);
  } else {
    Shell::RootWindowList root_window_list = Shell::GetAllRootWindows();
    for (size_t i = 0; i < root_window_list.size(); ++i)
      PositionWindowsFromRoot(root_window_list[i]);
  }
}

void WindowOverview::PositionWindowsFromRoot(aura::RootWindow* root_window) {
  std::vector<WindowSelectorWindow*> windows;
  for (WindowSelectorWindowList::iterator iter = windows_->begin();
       iter != windows_->end(); ++iter) {
    if ((*iter)->window()->GetRootWindow() == root_window)
      windows.push_back(*iter);
  }
  PositionWindowsOnRoot(root_window, windows);
}

void WindowOverview::PositionWindowsOnRoot(
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

void WindowOverview::InitializeSelectionWidget() {
  selection_widget_.reset(new views::Widget);
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.can_activate = false;
  params.keep_on_top = false;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
  params.parent = Shell::GetContainer(
      single_root_window_,
      internal::kShellWindowId_DefaultContainer);
  params.accept_events = false;
  selection_widget_->set_focus_on_creation(false);
  selection_widget_->Init(params);
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateSolidBackground(kWindowOverviewSelectionColor));
  selection_widget_->SetContentsView(content_view);
  selection_widget_->GetNativeWindow()->parent()->StackChildAtBottom(
      selection_widget_->GetNativeWindow());
  selection_widget_->Show();
  selection_widget_->GetNativeWindow()->layer()->SetOpacity(
      kWindowOverviewSelectionOpacity);
}

}  // namespace ash
