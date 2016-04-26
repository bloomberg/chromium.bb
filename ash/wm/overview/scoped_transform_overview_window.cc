// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_transform_overview_window.h"

#include <algorithm>
#include <vector>

#include "ash/screen_util.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// The opacity level that windows will be set to when they are restored.
const float kRestoreWindowOpacity = 1.0f;

aura::Window* GetTransientRoot(aura::Window* window) {
  while (::wm::GetTransientParent(window))
    window = ::wm::GetTransientParent(window);
  return window;
}

// An iterator class that traverses an aura::Window and all of it's transient
// descendants.
class TransientDescendantIterator {
 public:
  // Creates an empty iterator.
  TransientDescendantIterator();

  // Copy constructor required for iterator purposes.
  TransientDescendantIterator(
      const TransientDescendantIterator& other) = default;

  // Iterates over |root_window| and all of its transient descendants.
  // Note |root_window| must not have a transient parent.
  explicit TransientDescendantIterator(aura::Window* root_window);

  // Prefix increment operator.  This assumes there are more items (i.e.
  // *this != TransientDescendantIterator()).
  const TransientDescendantIterator& operator++();

  // Comparison for STL-based loops.
  bool operator!=(const TransientDescendantIterator& other) const;

  // Dereference operator for STL-compatible iterators.
  aura::Window* operator*() const;

 private:
  // The current window that |this| refers to. A NULL |current_window_| denotes
  // an empty iterator and is used as the last possible value in the traversal.
  aura::Window* current_window_;

  // Explicit assignment operator defined because an explicit copy constructor
  // is needed and therefore the DISALLOW_COPY_AND_ASSIGN macro cannot be used.
  TransientDescendantIterator& operator=(
      const TransientDescendantIterator& other) = default;
};

// Provides a virtual container implementing begin() and end() for a sequence of
// TransientDescendantIterators. This can be used in range-based for loops.
class TransientDescendantIteratorRange {
 public:
  explicit TransientDescendantIteratorRange(
      const TransientDescendantIterator& begin);

  // Copy constructor required for iterator purposes.
  TransientDescendantIteratorRange(
      const TransientDescendantIteratorRange& other) = default;

  const TransientDescendantIterator& begin() const { return begin_; }
  const TransientDescendantIterator& end() const { return end_; }

 private:
  TransientDescendantIterator begin_;
  TransientDescendantIterator end_;

  // Explicit assignment operator defined because an explicit copy constructor
  // is needed and therefore the DISALLOW_COPY_AND_ASSIGN macro cannot be used.
  TransientDescendantIteratorRange& operator=(
      const TransientDescendantIteratorRange& other) = default;
};

TransientDescendantIterator::TransientDescendantIterator()
  : current_window_(nullptr) {
}

TransientDescendantIterator::TransientDescendantIterator(
    aura::Window* root_window)
    : current_window_(root_window) {
  DCHECK(!::wm::GetTransientParent(root_window));
}

// Performs a pre-order traversal of the transient descendants.
const TransientDescendantIterator&
TransientDescendantIterator::operator++() {
  DCHECK(current_window_);

  const aura::Window::Windows& transient_children =
      ::wm::GetTransientChildren(current_window_);

  if (transient_children.size() > 0) {
    current_window_ = transient_children.front();
  } else {
    while (current_window_) {
      aura::Window* parent = ::wm::GetTransientParent(current_window_);
      if (!parent) {
        current_window_ = nullptr;
        break;
      }
      const aura::Window::Windows& transient_siblings =
          ::wm::GetTransientChildren(parent);
      aura::Window::Windows::const_iterator iter = std::find(
          transient_siblings.begin(),
          transient_siblings.end(),
          current_window_);
      ++iter;
      if (iter != transient_siblings.end()) {
        current_window_ = *iter;
        break;
      }
      current_window_ = ::wm::GetTransientParent(current_window_);
    }
  }
  return *this;
}

bool TransientDescendantIterator::operator!=(
    const TransientDescendantIterator& other) const {
  return current_window_ != other.current_window_;
}

aura::Window* TransientDescendantIterator::operator*() const {
  return current_window_;
}

TransientDescendantIteratorRange::TransientDescendantIteratorRange(
    const TransientDescendantIterator& begin)
    : begin_(begin) {
}

TransientDescendantIteratorRange GetTransientTreeIterator(
    aura::Window* window) {
  return TransientDescendantIteratorRange(
      TransientDescendantIterator(GetTransientRoot(window)));
}

}  // namespace

ScopedTransformOverviewWindow::ScopedTransformOverviewWindow(
        aura::Window* window)
    : window_(window),
      minimized_(window->GetProperty(aura::client::kShowStateKey) ==
                 ui::SHOW_STATE_MINIMIZED),
      ignored_by_shelf_(wm::GetWindowState(window)->ignored_by_shelf()),
      overview_started_(false),
      original_transform_(window->layer()->GetTargetTransform()),
      original_opacity_(window->layer()->GetTargetOpacity()) {
  DCHECK(window_);
}

ScopedTransformOverviewWindow::~ScopedTransformOverviewWindow() {
}

void ScopedTransformOverviewWindow::RestoreWindow() {
  ScopedAnimationSettings animation_settings_list;
  BeginScopedAnimation(
      OverviewAnimationType::OVERVIEW_ANIMATION_RESTORE_WINDOW,
      &animation_settings_list);
  SetTransform(window()->GetRootWindow(), original_transform_);

  ScopedOverviewAnimationSettings animation_settings(
      OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS,
      window_);
  gfx::Transform transform;
  if (minimized_ && window_->GetProperty(aura::client::kShowStateKey) !=
      ui::SHOW_STATE_MINIMIZED) {
    // Setting opacity 0 and visible false ensures that the property change
    // to SHOW_STATE_MINIMIZED will not animate the window from its original
    // bounds to the minimized position.
    // Hiding the window needs to be done before the target opacity is 0,
    // otherwise the layer's visibility will not be updated
    // (See VisibilityController::UpdateLayerVisibility).
    window_->Hide();
    window_->layer()->SetOpacity(0);
    window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  }
  wm::GetWindowState(window_)->set_ignored_by_shelf(ignored_by_shelf_);
  SetOpacity(original_opacity_);
}

void ScopedTransformOverviewWindow::BeginScopedAnimation(
    OverviewAnimationType animation_type,
    ScopedAnimationSettings* animation_settings) {
  for (const auto& window : GetTransientTreeIterator(window_)) {
    animation_settings->push_back(
        new ScopedOverviewAnimationSettings(animation_type, window));
  }
}

bool ScopedTransformOverviewWindow::Contains(const aura::Window* target) const {
  for (const auto& window : GetTransientTreeIterator(window_)) {
    if (window->Contains(target))
      return true;
  }
  return false;
}

gfx::Rect ScopedTransformOverviewWindow::GetTargetBoundsInScreen() const {
  gfx::Rect bounds;
  for (const auto& window : GetTransientTreeIterator(window_)) {
    // Ignore other window types when computing bounding box of window
    // selector target item.
    if (window != window_ && window->type() != ui::wm::WINDOW_TYPE_NORMAL &&
        window->type() != ui::wm::WINDOW_TYPE_PANEL) {
      continue;
    }
    bounds.Union(ScreenUtil::ConvertRectToScreen(window->parent(),
                                                 window->GetTargetBounds()));
  }
  return bounds;
}

void ScopedTransformOverviewWindow::ShowWindowIfMinimized() {
  if (minimized_ && window_->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED) {
    window_->Show();
  }
}

void ScopedTransformOverviewWindow::ShowWindowOnExit() {
  if (minimized_) {
    minimized_ = false;
    original_transform_ = gfx::Transform();
    original_opacity_ = kRestoreWindowOpacity;
  }
}

void ScopedTransformOverviewWindow::OnWindowDestroyed() {
  window_ = nullptr;
}

gfx::Rect ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
    const gfx::Rect& rect,
    const gfx::Rect& bounds) {
  DCHECK(!rect.IsEmpty());
  float scale = std::min(
      1.0f, std::min(static_cast<float>(bounds.width()) / rect.width(),
                     static_cast<float>(bounds.height()) / rect.height()));
  return gfx::Rect(bounds.x() + 0.5 * (bounds.width() - scale * rect.width()),
                   bounds.y() + 0.5 * (bounds.height() - scale * rect.height()),
                   rect.width() * scale,
                   rect.height() * scale);
}

gfx::Transform ScopedTransformOverviewWindow::GetTransformForRect(
    const gfx::Rect& src_rect,
    const gfx::Rect& dst_rect) {
  DCHECK(!src_rect.IsEmpty());
  gfx::Transform transform;
  transform.Translate(dst_rect.x() - src_rect.x(),
                      dst_rect.y() - src_rect.y());
  transform.Scale(static_cast<float>(dst_rect.width()) / src_rect.width(),
                  static_cast<float>(dst_rect.height()) / src_rect.height());
  return transform;
}

void ScopedTransformOverviewWindow::SetTransform(
    aura::Window* root_window,
    const gfx::Transform& transform) {
  DCHECK(overview_started_);

  gfx::Point target_origin(GetTargetBoundsInScreen().origin());

  for (const auto& window : GetTransientTreeIterator(window_)) {
    aura::Window* parent_window = window->parent();
    gfx::Point original_origin = ScreenUtil::ConvertRectToScreen(
        parent_window, window->GetTargetBounds()).origin();
    gfx::Transform new_transform = TransformAboutPivot(
        gfx::Point(target_origin.x() - original_origin.x(),
                   target_origin.y() - original_origin.y()),
        transform);
    window->SetTransform(new_transform);
  }
}

void ScopedTransformOverviewWindow::SetOpacity(float opacity) {
  for (const auto& window : GetTransientTreeIterator(window_)) {
    window->layer()->SetOpacity(opacity);
  }
}

void ScopedTransformOverviewWindow::Close() {
  aura::Window* window = GetTransientRoot(window_);
  views::Widget::GetWidgetForNativeView(window)->Close();
}

void ScopedTransformOverviewWindow::PrepareForOverview() {
  DCHECK(!overview_started_);
  overview_started_ = true;
  wm::GetWindowState(window_)->set_ignored_by_shelf(true);
  ShowWindowIfMinimized();
}

}  // namespace ash
