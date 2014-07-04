// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_panels.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/transparent_activate_window_button.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/views/controls/button/button.h"

namespace ash {

namespace {

// This class extends ScopedTransformOverviewMode to hide and show the callout
// widget for a panel window when entering / leaving overview mode, as well as
// to add a transparent button for each panel window.
class ScopedTransformPanelWindow : public ScopedTransformOverviewWindow {
 public:
  explicit ScopedTransformPanelWindow(aura::Window* window);
  virtual ~ScopedTransformPanelWindow();

  // ScopedTransformOverviewWindow overrides:
  virtual void PrepareForOverview() OVERRIDE;

  virtual void SetTransform(
      aura::Window* root_window,
      const gfx::Transform& transform,
      bool animate) OVERRIDE;

 private:
  // Returns the panel window bounds after the transformation.
  gfx::Rect GetTransformedBounds();

  scoped_ptr<TransparentActivateWindowButton> window_button_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTransformPanelWindow);
};

ScopedTransformPanelWindow::ScopedTransformPanelWindow(aura::Window* window)
    : ScopedTransformOverviewWindow(window) {
}

ScopedTransformPanelWindow::~ScopedTransformPanelWindow() {
}

void ScopedTransformPanelWindow::PrepareForOverview() {
  ScopedTransformOverviewWindow::PrepareForOverview();
  window_button_.reset(new TransparentActivateWindowButton(window()));
}

void ScopedTransformPanelWindow::SetTransform(
    aura::Window* root_window,
    const gfx::Transform& transform,
    bool animate) {
  ScopedTransformOverviewWindow::SetTransform(root_window, transform, animate);
  window_button_->SetBounds(GetTransformedBounds());
}

gfx::Rect ScopedTransformPanelWindow::GetTransformedBounds() {
  gfx::RectF bounds(ScreenUtil::ConvertRectToScreen(
          window()->GetRootWindow(), window()->layer()->bounds()));
  gfx::Transform new_transform;
  new_transform.Translate(bounds.x(),
                          bounds.y());
  new_transform.PreconcatTransform(window()->layer()->GetTargetTransform());
  new_transform.Translate(-bounds.x(),
                          -bounds.y());
  new_transform.TransformRect(&bounds);
  return ToEnclosingRect(bounds);
}

}  // namespace

WindowSelectorPanels::WindowSelectorPanels(aura::Window* panels_root_window)
    : panels_root_window_(panels_root_window) {
  static_cast<PanelLayoutManager*>(
      Shell::GetContainer(panels_root_window_, kShellWindowId_PanelContainer)->
          layout_manager())->SetShowCalloutWidgets(false);
}

WindowSelectorPanels::~WindowSelectorPanels() {
  static_cast<PanelLayoutManager*>(
      Shell::GetContainer(panels_root_window_, kShellWindowId_PanelContainer)->
          layout_manager())->SetShowCalloutWidgets(true);
  for (WindowList::iterator iter = transform_windows_.begin();
      iter != transform_windows_.end(); iter++) {
    (*iter)->window()->RemoveObserver(this);
  }
}

void WindowSelectorPanels::AddWindow(aura::Window* window) {
  DCHECK(window->GetRootWindow() == panels_root_window_);
  window->AddObserver(this);
  transform_windows_.push_back(new ScopedTransformPanelWindow(window));
}

aura::Window* WindowSelectorPanels::GetRootWindow() {
  return transform_windows_.front()->window()->GetRootWindow();
}

bool WindowSelectorPanels::HasSelectableWindow(const aura::Window* window) {
  for (WindowList::const_iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    if ((*iter)->window() == window)
      return true;
  }
  return false;
}

bool WindowSelectorPanels::Contains(const aura::Window* target) {
  for (WindowList::const_iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    if ((*iter)->Contains(target))
      return true;
  }
  return false;
}

void WindowSelectorPanels::RestoreWindowOnExit(aura::Window* window) {
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    if ((*iter)->Contains(window)) {
      (*iter)->RestoreWindowOnExit();
      break;
    }
  }
}

aura::Window* WindowSelectorPanels::SelectionWindow() {
  return transform_windows_.front()->window();
}

void WindowSelectorPanels::RemoveWindow(const aura::Window* window) {
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    if ((*iter)->window() == window) {
      (*iter)->window()->RemoveObserver(this);
      (*iter)->OnWindowDestroyed();
      transform_windows_.erase(iter);
      break;
    }
  }
  WindowSelectorItem::RemoveWindow(window);
}

bool WindowSelectorPanels::empty() const {
  return transform_windows_.empty();
}

void WindowSelectorPanels::PrepareForOverview() {
  // |panel_windows| will hold all the windows in the panel container, sorted
  // according to their stacking order.
  const aura::Window::Windows panels =
      transform_windows_.front()->window()->parent()->children();

  // Call PrepareForOverview() in the reverse stacking order so that the
  // transparent windows that handle the events are in the correct stacking
  // order.
  size_t transformed_windows = 0;
  for (aura::Window::Windows::const_reverse_iterator iter = panels.rbegin();
      iter != panels.rend(); iter++) {
    for (size_t j = 0; j < transform_windows_.size(); ++j) {
      if (transform_windows_[j]->window() == (*iter)) {
        transform_windows_[j]->PrepareForOverview();
        transformed_windows++;
      }
    }
  }
  DCHECK(transformed_windows == transform_windows_.size());
}

void WindowSelectorPanels::SetItemBounds(aura::Window* root_window,
                                         const gfx::Rect& target_bounds,
                                         bool animate) {
  gfx::Rect bounding_rect;
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    bounding_rect.Union((*iter)->GetBoundsInScreen());
  }
  set_bounds(ScopedTransformOverviewWindow::
      ShrinkRectToFitPreservingAspectRatio(bounding_rect, target_bounds));
  gfx::Transform bounding_transform =
      ScopedTransformOverviewWindow::GetTransformForRect(bounding_rect,
                                                         bounds());
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); ++iter) {
    gfx::Transform transform;
    gfx::Rect bounds = (*iter)->GetBoundsInScreen();
    transform.Translate(bounding_rect.x() - bounds.x(),
                        bounding_rect.y() - bounds.y());
    transform.PreconcatTransform(bounding_transform);
    transform.Translate(bounds.x() - bounding_rect.x(),
                        bounds.y() - bounding_rect.y());
    (*iter)->SetTransform(root_window, transform, animate);
  }
}

void WindowSelectorPanels::SetOpacity(float opacity) {
  // TODO(flackr): find a way to make panels that are hidden behind other panels
  // look nice.
  for (WindowList::iterator iter = transform_windows_.begin();
       iter != transform_windows_.end(); iter++) {
    (*iter)->window()->layer()->SetOpacity(opacity);
  }
  WindowSelectorItem::SetOpacity(opacity);
}

}  // namespace ash
