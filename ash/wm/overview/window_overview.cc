// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_overview.h"

#include <algorithm>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/switchable_windows.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_item.h"
#include "base/metrics/histogram.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Conceptually the window overview is a table or grid of cells having this
// fixed aspect ratio. The number of columns is determined by maximizing the
// area of them based on the number of windows.
const float kCardAspectRatio = 4.0f / 3.0f;

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
const int kWindowMargin = 30;

// The minimum number of cards along the major axis (i.e. horizontally on a
// landscape orientation).
const int kMinCardsMajor = 3;

// The duration of transition animations on the overview selector.
const int kOverviewSelectorTransitionMilliseconds = 100;

// The color and opacity of the overview selector.
const SkColor kWindowOverviewSelectionColor = SK_ColorBLACK;
const float kWindowOverviewSelectionOpacity = 0.5f;

// The padding or amount of the window selector widget visible around the edges
// of the currently selected window.
const int kWindowOverviewSelectionPadding = 25;

// A comparator for locating a given target window.
struct WindowSelectorItemComparator
    : public std::unary_function<WindowSelectorItem*, bool> {
  explicit WindowSelectorItemComparator(const aura::Window* target_window)
      : target(target_window) {
  }

  bool operator()(WindowSelectorItem* window) const {
    return window->TargetedWindow(target) != NULL;
  }

  const aura::Window* target;
};

// An observer which holds onto the passed widget until the animation is
// complete.
class CleanupWidgetAfterAnimationObserver : public ui::LayerAnimationObserver {
 public:
  explicit CleanupWidgetAfterAnimationObserver(
      scoped_ptr<views::Widget> widget);

  // ui::LayerAnimationObserver:
  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE;

 private:
  virtual ~CleanupWidgetAfterAnimationObserver();

  scoped_ptr<views::Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(CleanupWidgetAfterAnimationObserver);
};

CleanupWidgetAfterAnimationObserver::CleanupWidgetAfterAnimationObserver(
    scoped_ptr<views::Widget> widget)
    : widget_(widget.Pass()) {
  widget_->GetNativeWindow()->layer()->GetAnimator()->AddObserver(this);
}

CleanupWidgetAfterAnimationObserver::~CleanupWidgetAfterAnimationObserver() {
  widget_->GetNativeWindow()->layer()->GetAnimator()->RemoveObserver(this);
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  delete this;
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {
  delete this;
}

void CleanupWidgetAfterAnimationObserver::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
}

}  // namespace

WindowOverview::WindowOverview(WindowSelector* window_selector,
                               WindowSelectorItemList* windows,
                               aura::Window* single_root_window)
    : window_selector_(window_selector),
      windows_(windows),
      selection_index_(0),
      single_root_window_(single_root_window),
      overview_start_time_(base::Time::Now()),
      cursor_client_(NULL) {
  Shell* shell = Shell::GetInstance();
  shell->OnOverviewModeStarting();
  for (WindowSelectorItemList::iterator iter = windows_->begin();
       iter != windows_->end(); ++iter) {
    (*iter)->PrepareForOverview();
  }
  PositionWindows();
  DCHECK(!windows_->empty());
  cursor_client_ = aura::client::GetCursorClient(
      windows_->front()->GetRootWindow());
  if (cursor_client_) {
    cursor_client_->SetCursor(ui::kCursorPointer);
    cursor_client_->ShowCursor();
    // TODO(flackr): Only prevent cursor changes for windows in the overview.
    // This will be easier to do without exposing the overview mode code if the
    // cursor changes are moved to ToplevelWindowEventHandler::HandleMouseMoved
    // as suggested there.
    cursor_client_->LockCursor();
  }
  shell->PrependPreTargetHandler(this);
  shell->metrics()->RecordUserMetricsAction(UMA_WINDOW_OVERVIEW);
  HideAndTrackNonOverviewWindows();
}

WindowOverview::~WindowOverview() {
  const aura::WindowTracker::Windows hidden_windows = hidden_windows_.windows();
  for (aura::WindowTracker::Windows::const_iterator iter =
       hidden_windows.begin(); iter != hidden_windows.end(); ++iter) {
    ui::ScopedLayerAnimationSettings settings(
        (*iter)->layer()->GetAnimator());
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        ScopedTransformOverviewWindow::kTransitionMilliseconds));
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    (*iter)->Show();
    (*iter)->layer()->SetOpacity(1);
  }
  if (cursor_client_)
    cursor_client_->UnlockCursor();
  ash::Shell* shell = ash::Shell::GetInstance();
  shell->RemovePreTargetHandler(this);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "Ash.WindowSelector.TimeInOverview",
      base::Time::Now() - overview_start_time_);
  shell->OnOverviewModeEnding();
}

void WindowOverview::SetSelection(size_t index) {
  gfx::Rect target_bounds(GetSelectionBounds(index));

  if (selection_widget_) {
    // If the selection widget is already active, determine the animation to
    // use to animate the widget to the new bounds.
    int change = static_cast<int>(index) - static_cast<int>(selection_index_);
    int windows = static_cast<int>(windows_->size());
    // If moving from the first to the last or last to the first index,
    // convert the delta to be +/- 1.
    if (windows > 2 && abs(change) == windows - 1) {
      if (change < 0)
        change += windows;
      else
        change -= windows;
    }
    if (selection_index_ < windows_->size() &&
        (*windows_)[selection_index_]->target_bounds().y() !=
            (*windows_)[index]->target_bounds().y() &&
        abs(change) == 1) {
      // The selection has changed forward or backwards by one with a change
      // in the height of the target. In this case create a new selection widget
      // to fade in on the new position and animate and fade out the old one.
      gfx::Display dst_display = gfx::Screen::GetScreenFor(
          selection_widget_->GetNativeWindow())->GetDisplayMatching(
              target_bounds);
      gfx::Vector2d fade_out_direction(
          change * ((*windows_)[selection_index_]->target_bounds().width() +
                    2 * kWindowMargin), 0);
      aura::Window* old_selection = selection_widget_->GetNativeWindow();

      // CleanupWidgetAfterAnimationObserver will delete itself (and the
      // widget) when the animation is complete.
      new CleanupWidgetAfterAnimationObserver(selection_widget_.Pass());
      ui::ScopedLayerAnimationSettings animation_settings(
          old_selection->layer()->GetAnimator());
      animation_settings.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(
              kOverviewSelectorTransitionMilliseconds));
      animation_settings.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      old_selection->SetBoundsInScreen(
          GetSelectionBounds(selection_index_) + fade_out_direction,
          dst_display);
      old_selection->layer()->SetOpacity(0);
      InitializeSelectionWidget();
      selection_widget_->GetNativeWindow()->SetBoundsInScreen(
          target_bounds - fade_out_direction, dst_display);
    }
    ui::ScopedLayerAnimationSettings animation_settings(
        selection_widget_->GetNativeWindow()->layer()->GetAnimator());
    animation_settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        kOverviewSelectorTransitionMilliseconds));
    animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    selection_widget_->SetBounds(target_bounds);
    selection_widget_->GetNativeWindow()->layer()->SetOpacity(
        kWindowOverviewSelectionOpacity);
  } else {
    InitializeSelectionWidget();
    selection_widget_->SetBounds(target_bounds);
    selection_widget_->GetNativeWindow()->layer()->SetOpacity(
        kWindowOverviewSelectionOpacity);
  }
  selection_index_ = index;
}

void WindowOverview::OnWindowsChanged() {
  PositionWindows();
}

void WindowOverview::MoveToSingleRootWindow(aura::Window* root_window) {
  single_root_window_ = root_window;
  PositionWindows();
}

void WindowOverview::OnKeyEvent(ui::KeyEvent* event) {
  if (GetTargetedWindow(static_cast<aura::Window*>(event->target())))
    event->StopPropagation();
  if (event->type() != ui::ET_KEY_PRESSED)
    return;

  if (event->key_code() == ui::VKEY_ESCAPE)
    window_selector_->CancelSelection();
}

void WindowOverview::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = GetEventTarget(event);
  if (!target)
    return;

  event->StopPropagation();
  if (event->type() != ui::ET_MOUSE_RELEASED)
    return;

  window_selector_->SelectWindow(target);
}

void WindowOverview::OnScrollEvent(ui::ScrollEvent* event) {
  // Set the handled flag to prevent delivering scroll events to the window but
  // still allowing other pretarget handlers to process the scroll event.
  if (GetTargetedWindow(static_cast<aura::Window*>(event->target())))
    event->SetHandled();
}

void WindowOverview::OnTouchEvent(ui::TouchEvent* event) {
  // Existing touches should be allowed to continue. This prevents getting
  // stuck in a gesture or with pressed fingers being tracked elsewhere.
  if (event->type() != ui::ET_TOUCH_PRESSED)
    return;

  aura::Window* target = GetEventTarget(event);
  if (!target)
    return;

  // TODO(flackr): StopPropogation prevents generation of gesture events.
  // We should find a better way to prevent events from being delivered to
  // the window, perhaps a transparent window in front of the target window
  // or using EventClientImpl::CanProcessEventsWithinSubtree and then a tap
  // gesture could be used to activate the window.
  event->SetHandled();
  window_selector_->SelectWindow(target);
}

aura::Window* WindowOverview::GetEventTarget(ui::LocatedEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  // If the target window doesn't actually contain the event location (i.e.
  // mouse down over the window and mouse up elsewhere) then do not select the
  // window.
  if (!target->ContainsPoint(event->location()))
    return NULL;

  return GetTargetedWindow(target);
}

aura::Window* WindowOverview::GetTargetedWindow(aura::Window* window) {
  for (WindowSelectorItemList::iterator iter = windows_->begin();
       iter != windows_->end(); ++iter) {
    aura::Window* selected = (*iter)->TargetedWindow(window);
    if (selected)
      return selected;
  }
  return NULL;
}

void WindowOverview::HideAndTrackNonOverviewWindows() {
  // Add the windows to hidden_windows first so that if any are destroyed
  // while hiding them they are tracked.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::const_iterator root_iter = root_windows.begin();
       root_iter != root_windows.end(); ++root_iter) {
    for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
      aura::Window* container = Shell::GetContainer(*root_iter,
          kSwitchableWindowContainerIds[i]);
      for (aura::Window::Windows::const_iterator iter =
           container->children().begin(); iter != container->children().end();
           ++iter) {
        if (GetTargetedWindow(*iter) || !(*iter)->IsVisible())
          continue;
        hidden_windows_.Add(*iter);
      }
    }
  }

  // Copy the window list as it can change during iteration.
  const aura::WindowTracker::Windows hidden_windows(hidden_windows_.windows());
  for (aura::WindowTracker::Windows::const_iterator iter =
       hidden_windows.begin(); iter != hidden_windows.end(); ++iter) {
    if (!hidden_windows_.Contains(*iter))
      continue;
    ui::ScopedLayerAnimationSettings settings(
        (*iter)->layer()->GetAnimator());
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
        ScopedTransformOverviewWindow::kTransitionMilliseconds));
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    (*iter)->Hide();
    // Hiding the window can result in it being destroyed.
    if (!hidden_windows_.Contains(*iter))
      continue;
    (*iter)->layer()->SetOpacity(0);
  }
}

void WindowOverview::PositionWindows() {
  if (single_root_window_) {
    std::vector<WindowSelectorItem*> windows;
    for (WindowSelectorItemList::iterator iter = windows_->begin();
         iter != windows_->end(); ++iter) {
      windows.push_back(*iter);
    }
    PositionWindowsOnRoot(single_root_window_, windows);
  } else {
    aura::Window::Windows root_window_list = Shell::GetAllRootWindows();
    for (size_t i = 0; i < root_window_list.size(); ++i)
      PositionWindowsFromRoot(root_window_list[i]);
  }
}

void WindowOverview::PositionWindowsFromRoot(aura::Window* root_window) {
  std::vector<WindowSelectorItem*> windows;
  for (WindowSelectorItemList::iterator iter = windows_->begin();
       iter != windows_->end(); ++iter) {
    if ((*iter)->GetRootWindow() == root_window)
      windows.push_back(*iter);
  }
  PositionWindowsOnRoot(root_window, windows);
}

void WindowOverview::PositionWindowsOnRoot(
    aura::Window* root_window,
    const std::vector<WindowSelectorItem*>& windows) {
  if (windows.empty())
    return;

  gfx::Size window_size;
  gfx::Rect total_bounds = ScreenUtil::ConvertRectToScreen(
      root_window,
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(
          Shell::GetContainer(root_window, kShellWindowId_DefaultContainer)));

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
    windows[i]->SetBounds(root_window, target_bounds);
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
  params.parent = Shell::GetContainer(single_root_window_
                                          ? single_root_window_
                                          : windows_->front()->GetRootWindow(),
                                      kShellWindowId_DefaultContainer);
  params.accept_events = false;
  selection_widget_->set_focus_on_creation(false);
  selection_widget_->Init(params);
  views::View* content_view = new views::View;
  content_view->set_background(
      views::Background::CreateSolidBackground(kWindowOverviewSelectionColor));
  selection_widget_->SetContentsView(content_view);
  selection_widget_->Show();
  selection_widget_->GetNativeWindow()->parent()->StackChildAtBottom(
      selection_widget_->GetNativeWindow());
  selection_widget_->GetNativeWindow()->layer()->SetOpacity(0);
}

gfx::Rect WindowOverview::GetSelectionBounds(size_t index) {
  gfx::Rect bounds((*windows_)[index]->bounds());
  bounds.Inset(-kWindowOverviewSelectionPadding,
               -kWindowOverviewSelectionPadding);
  return bounds;
}

}  // namespace ash
