// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_selector.h"

#include <algorithm>

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_selector_delegate.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/display.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/corewm/shadow_types.h"
#include "ui/views/corewm/window_animations.h"
#include "ui/views/corewm/window_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

const float kCardAspectRatio = 4.0f / 3.0f;
const int kWindowMargin = 30;
const int kMinCardsMajor = 3;
const int kOverviewTransitionMilliseconds = 100;
const SkColor kWindowSelectorSelectionColor = SK_ColorBLACK;
const float kWindowSelectorSelectionOpacity = 0.5f;
const int kWindowSelectorSelectionPadding = 15;

// Creates a copy of |window| with |recreated_layer| in the |target_root|.
views::Widget* CreateCopyOfWindow(aura::RootWindow* target_root,
                                  aura::Window* src_window,
                                  ui::Layer* recreated_layer) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = src_window->parent();
  params.can_activate = false;
  params.keep_on_top = true;
  widget->set_focus_on_creation(false);
  widget->Init(params);
  widget->SetVisibilityChangedAnimationsEnabled(false);
  std::string name = src_window->name() + " (Copy)";
  widget->GetNativeWindow()->SetName(name);
  views::corewm::SetShadowType(widget->GetNativeWindow(),
                               views::corewm::SHADOW_TYPE_RECTANGULAR);

  // Set the bounds in the target root window.
  gfx::Display target_display =
      Shell::GetScreen()->GetDisplayNearestWindow(target_root);
  aura::client::ScreenPositionClient* screen_position_client =
      aura::client::GetScreenPositionClient(src_window->GetRootWindow());
  if (screen_position_client && target_display.is_valid()) {
    screen_position_client->SetBounds(widget->GetNativeWindow(),
        src_window->GetBoundsInScreen(), target_display);
  } else {
    widget->SetBounds(src_window->GetBoundsInScreen());
  }
  widget->StackAbove(src_window);

  // Move the |recreated_layer| to the newly created window.
  recreated_layer->set_delegate(src_window->layer()->delegate());
  gfx::Rect layer_bounds = recreated_layer->bounds();
  layer_bounds.set_origin(gfx::Point(0, 0));
  recreated_layer->SetBounds(layer_bounds);
  recreated_layer->SetVisible(false);
  recreated_layer->parent()->Remove(recreated_layer);

  aura::Window* window = widget->GetNativeWindow();
  recreated_layer->SetVisible(true);
  window->layer()->Add(recreated_layer);
  window->layer()->StackAtTop(recreated_layer);
  window->layer()->SetOpacity(1);
  window->Show();
  return widget;
}

// An observer which closes the widget and deletes the layer after an
// animation finishes.
class CleanupWidgetAfterAnimationObserver : public ui::LayerAnimationObserver {
 public:
  CleanupWidgetAfterAnimationObserver(views::Widget* widget, ui::Layer* layer);

  virtual void OnLayerAnimationEnded(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) OVERRIDE;

 protected:
  virtual ~CleanupWidgetAfterAnimationObserver();

 private:
  views::Widget* widget_;
  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(CleanupWidgetAfterAnimationObserver);
};

CleanupWidgetAfterAnimationObserver::CleanupWidgetAfterAnimationObserver(
        views::Widget* widget,
        ui::Layer* layer)
    : widget_(widget),
      layer_(layer) {
  widget_->GetNativeWindow()->layer()->GetAnimator()->AddObserver(this);
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

CleanupWidgetAfterAnimationObserver::~CleanupWidgetAfterAnimationObserver() {
  widget_->GetNativeWindow()->layer()->GetAnimator()->RemoveObserver(this);
  widget_->Close();
  widget_ = NULL;
  if (layer_) {
    views::corewm::DeepDeleteLayers(layer_);
    layer_ = NULL;
  }
}

// The animation settings used for window selector animations.
class WindowSelectorAnimationSettings
    : public ui::ScopedLayerAnimationSettings {
 public:
  WindowSelectorAnimationSettings(aura::Window* window) :
      ui::ScopedLayerAnimationSettings(window->layer()->GetAnimator()) {
    SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kOverviewTransitionMilliseconds));
  }

  virtual ~WindowSelectorAnimationSettings() {
  }
};

}  // namespace

// TODO(flackr): Split up into separate file under subdirectory in ash/wm.
class WindowSelectorWindow {
 public:
  explicit WindowSelectorWindow(aura::Window* window);
  virtual ~WindowSelectorWindow();

  aura::Window* window() { return window_; }
  const aura::Window* window() const { return window_; }

  // Returns true if this window selector window contains the |target|. This is
  // used to determine if an event targetted this window.
  bool Contains(const aura::Window* target) const;

  // Restores this window on exit rather than returning it to a minimized state
  // if it was minimized on entering overview mode.
  void RestoreWindowOnExit();

  // Informs the WindowSelectorWindow that the window being watched was
  // destroyed. This resets the internal window pointer to avoid calling
  // anything on the window at destruction time.
  void OnWindowDestroyed();

  // Applies a transform to the window to fit within |target_bounds| while
  // maintaining its aspect ratio.
  void TransformToFitBounds(aura::RootWindow* root_window,
                            const gfx::Rect& target_bounds);

  gfx::Rect bounds() { return fit_bounds_; }

 private:
  // A weak pointer to the real window in the overview.
  aura::Window* window_;

  // A copy of the window used to transition the window to another root.
  views::Widget* window_copy_;

  // A weak pointer to a deep copy of the window's layers.
  ui::Layer* layer_;

  // If true, the window was minimized and should be restored if the window
  // was not selected.
  bool minimized_;

  // The original transform of the window before entering overview mode.
  gfx::Transform original_transform_;

  // The bounds this window is fit to.
  gfx::Rect fit_bounds_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorWindow);
};

WindowSelectorWindow::WindowSelectorWindow(aura::Window* window)
    : window_(window),
      window_copy_(NULL),
      layer_(NULL),
      minimized_(window->GetProperty(aura::client::kShowStateKey) ==
                 ui::SHOW_STATE_MINIMIZED),
      original_transform_(window->layer()->transform()) {
  if (minimized_)
    window_->Show();
}

WindowSelectorWindow::~WindowSelectorWindow() {
  if (window_) {
    WindowSelectorAnimationSettings animation_settings(window_);
    gfx::Transform transform;
    window_->SetTransform(original_transform_);
    if (minimized_) {
      // Setting opacity 0 and visible false ensures that the property change
      // to SHOW_STATE_MINIMIZED will not animate the window from its original
      // bounds to the minimized position.
      window_->layer()->SetOpacity(0);
      window_->layer()->SetVisible(false);
      window_->SetProperty(aura::client::kShowStateKey,
                           ui::SHOW_STATE_MINIMIZED);
    }
  }
  // If a copy of the window was created, clean it up.
  if (window_copy_) {
    if (window_) {
      // If the initial window wasn't destroyed, the copy needs to be animated
      // out. CleanupWidgetAfterAnimationObserver will destroy the widget and
      // layer after the animation is complete.
      new CleanupWidgetAfterAnimationObserver(window_copy_, layer_);
      WindowSelectorAnimationSettings animation_settings(
          window_copy_->GetNativeWindow());
      window_copy_->GetNativeWindow()->SetTransform(original_transform_);
    } else {
      window_copy_->Close();
      if (layer_)
        views::corewm::DeepDeleteLayers(layer_);
    }
    window_copy_ = NULL;
    layer_ = NULL;
  }
}

bool WindowSelectorWindow::Contains(const aura::Window* window) const {
  if (window_copy_ && window_copy_->GetNativeWindow()->Contains(window))
    return true;
  return window_->Contains(window);
}

void WindowSelectorWindow::RestoreWindowOnExit() {
  minimized_ = false;
  original_transform_ = gfx::Transform();
}

void WindowSelectorWindow::OnWindowDestroyed() {
  window_ = NULL;
}

void WindowSelectorWindow::TransformToFitBounds(
    aura::RootWindow* root_window,
    const gfx::Rect& target_bounds) {
  fit_bounds_ = target_bounds;
  const gfx::Rect bounds = window_->GetBoundsInScreen();
  float scale = std::min(1.0f,
      std::min(static_cast<float>(target_bounds.width()) / bounds.width(),
               static_cast<float>(target_bounds.height()) / bounds.height()));
  gfx::Transform transform;
  gfx::Vector2d offset(
      0.5 * (target_bounds.width() - scale * bounds.width()),
      0.5 * (target_bounds.height() - scale * bounds.height()));
  transform.Translate(target_bounds.x() - bounds.x() + offset.x(),
                      target_bounds.y() - bounds.y() + offset.y());
  transform.Scale(scale, scale);
  if (root_window != window_->GetRootWindow()) {
    if (!window_copy_) {
      DCHECK(!layer_);
      layer_ = views::corewm::RecreateWindowLayers(window_, true);
      window_copy_ = CreateCopyOfWindow(root_window, window_, layer_);
    }
    WindowSelectorAnimationSettings animation_settings(
        window_copy_->GetNativeWindow());
    window_copy_->GetNativeWindow()->SetTransform(transform);
  }
  WindowSelectorAnimationSettings animation_settings(window_);
  window_->SetTransform(transform);
}

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

void WindowSelector::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() != ui::ET_GESTURE_TAP)
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
    WindowSelectorAnimationSettings animation_settings(
        selection_widget_->GetNativeWindow());
    selection_widget_->SetBounds(target_bounds);
  } else {
    selection_widget_->SetBounds(target_bounds);
  }
}

}  // namespace ash
