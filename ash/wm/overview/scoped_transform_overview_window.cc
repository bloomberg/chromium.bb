// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_transform_overview_window.h"

#include <algorithm>
#include <vector>

#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_window.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_overview_animation_settings_factory.h"
#include "ash/wm/overview/window_selector_item.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform_util.h"

using WmWindows = std::vector<ash::wm::WmWindow*>;

namespace ash {

namespace {

// The opacity level that windows will be set to when they are restored.
const float kRestoreWindowOpacity = 1.0f;

wm::WmWindow* GetTransientRoot(wm::WmWindow* window) {
  while (window->GetTransientParent())
    window = window->GetTransientParent();
  return window;
}

std::unique_ptr<ScopedOverviewAnimationSettings>
CreateScopedOverviewAnimationSettings(OverviewAnimationType animation_type,
                                      wm::WmWindow* window) {
  return ScopedOverviewAnimationSettingsFactory::Get()
      ->CreateOverviewAnimationSettings(animation_type, window);
}

// An iterator class that traverses a wm::WmWindow and all of its transient
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
  explicit TransientDescendantIterator(wm::WmWindow* root_window);

  // Prefix increment operator.  This assumes there are more items (i.e.
  // *this != TransientDescendantIterator()).
  const TransientDescendantIterator& operator++();

  // Comparison for STL-based loops.
  bool operator!=(const TransientDescendantIterator& other) const;

  // Dereference operator for STL-compatible iterators.
  wm::WmWindow* operator*() const;

 private:
  // Explicit assignment operator defined because an explicit copy constructor
  // is needed and therefore the DISALLOW_COPY_AND_ASSIGN macro cannot be used.
  TransientDescendantIterator& operator=(
      const TransientDescendantIterator& other) = default;

  // The current window that |this| refers to. A null |current_window_| denotes
  // an empty iterator and is used as the last possible value in the traversal.
  wm::WmWindow* current_window_;
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
  // Explicit assignment operator defined because an explicit copy constructor
  // is needed and therefore the DISALLOW_COPY_AND_ASSIGN macro cannot be used.
  TransientDescendantIteratorRange& operator=(
      const TransientDescendantIteratorRange& other) = default;

  TransientDescendantIterator begin_;
  TransientDescendantIterator end_;
};

TransientDescendantIterator::TransientDescendantIterator()
  : current_window_(nullptr) {
}

TransientDescendantIterator::TransientDescendantIterator(
    wm::WmWindow* root_window)
    : current_window_(root_window) {
  DCHECK(!root_window->GetTransientParent());
}

// Performs a pre-order traversal of the transient descendants.
const TransientDescendantIterator&
TransientDescendantIterator::operator++() {
  DCHECK(current_window_);

  const WmWindows transient_children = current_window_->GetTransientChildren();

  if (!transient_children.empty()) {
    current_window_ = transient_children.front();
  } else {
    while (current_window_) {
      wm::WmWindow* parent = current_window_->GetTransientParent();
      if (!parent) {
        current_window_ = nullptr;
        break;
      }
      const WmWindows transient_siblings = parent->GetTransientChildren();
      auto iter = std::find(transient_siblings.begin(),
                            transient_siblings.end(), current_window_);
      ++iter;
      if (iter != transient_siblings.end()) {
        current_window_ = *iter;
        break;
      }
      current_window_ = current_window_->GetTransientParent();
    }
  }
  return *this;
}

bool TransientDescendantIterator::operator!=(
    const TransientDescendantIterator& other) const {
  return current_window_ != other.current_window_;
}

wm::WmWindow* TransientDescendantIterator::operator*() const {
  return current_window_;
}

TransientDescendantIteratorRange::TransientDescendantIteratorRange(
    const TransientDescendantIterator& begin)
    : begin_(begin) {
}

TransientDescendantIteratorRange GetTransientTreeIterator(
    wm::WmWindow* window) {
  return TransientDescendantIteratorRange(
      TransientDescendantIterator(GetTransientRoot(window)));
}

}  // namespace

ScopedTransformOverviewWindow::ScopedTransformOverviewWindow(
    wm::WmWindow* window)
    : window_(window),
      minimized_(window->GetShowState() == ui::SHOW_STATE_MINIMIZED),
      ignored_by_shelf_(window->GetWindowState()->ignored_by_shelf()),
      overview_started_(false),
      original_transform_(window->GetTargetTransform()),
      original_opacity_(window->GetTargetOpacity()) {}

ScopedTransformOverviewWindow::~ScopedTransformOverviewWindow() {
}

void ScopedTransformOverviewWindow::RestoreWindow() {
  ScopedAnimationSettings animation_settings_list;
  BeginScopedAnimation(
      OverviewAnimationType::OVERVIEW_ANIMATION_RESTORE_WINDOW,
      &animation_settings_list);
  SetTransform(window()->GetRootWindow(), original_transform_);

  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings =
      CreateScopedOverviewAnimationSettings(
          OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS,
          window_);
  gfx::Transform transform;
  if (minimized_ && window_->GetShowState() != ui::SHOW_STATE_MINIMIZED) {
    // Setting opacity 0 and visible false ensures that the property change
    // to SHOW_STATE_MINIMIZED will not animate the window from its original
    // bounds to the minimized position.
    // Hiding the window needs to be done before the target opacity is 0,
    // otherwise the layer's visibility will not be updated
    // (See VisibilityController::UpdateLayerVisibility).
    window_->Hide();
    window_->SetOpacity(0);
    window_->SetShowState(ui::SHOW_STATE_MINIMIZED);
  }
  window_->GetWindowState()->set_ignored_by_shelf(ignored_by_shelf_);
  SetOpacity(original_opacity_);
}

void ScopedTransformOverviewWindow::BeginScopedAnimation(
    OverviewAnimationType animation_type,
    ScopedAnimationSettings* animation_settings) {
  for (const auto& window : GetTransientTreeIterator(window_)) {
    animation_settings->push_back(
        CreateScopedOverviewAnimationSettings(animation_type, window));
  }
}

bool ScopedTransformOverviewWindow::Contains(const wm::WmWindow* target) const {
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
    if (window != window_ && window->GetType() != ui::wm::WINDOW_TYPE_NORMAL &&
        window->GetType() != ui::wm::WINDOW_TYPE_PANEL) {
      continue;
    }
    bounds.Union(
        window->GetParent()->ConvertRectToScreen(window->GetTargetBounds()));
  }
  return bounds;
}

void ScopedTransformOverviewWindow::ShowWindowIfMinimized() {
  if (minimized_ && window_->GetShowState() == ui::SHOW_STATE_MINIMIZED)
    window_->Show();
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
    const gfx::Rect& bounds,
    int top_view_inset,
    int title_height) {
  DCHECK(!rect.IsEmpty());
  DCHECK_LE(top_view_inset, rect.height());
  DCHECK_LE(title_height, bounds.height());
  float scale = std::min(
      1.0f, std::min(static_cast<float>(bounds.width()) / rect.width(),
                     static_cast<float>((bounds.height() - title_height)) /
                         (rect.height() - top_view_inset)));
  return gfx::Rect(
      bounds.x() + 0.5 * (bounds.width() - scale * rect.width()),
      bounds.y() + title_height - scale * top_view_inset +
          0.5 * (bounds.height() -
                 (title_height + scale * (rect.height() - top_view_inset))),
      rect.width() * scale, rect.height() * scale);
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
    wm::WmWindow* root_window,
    const gfx::Transform& transform) {
  DCHECK(overview_started_);

  gfx::Point target_origin(GetTargetBoundsInScreen().origin());

  for (const auto& window : GetTransientTreeIterator(window_)) {
    wm::WmWindow* parent_window = window->GetParent();
    gfx::Point original_origin =
        parent_window->ConvertRectToScreen(window->GetTargetBounds()).origin();
    gfx::Transform new_transform = TransformAboutPivot(
        gfx::Point(target_origin.x() - original_origin.x(),
                   target_origin.y() - original_origin.y()),
        transform);
    window->SetTransform(new_transform);
  }
}

void ScopedTransformOverviewWindow::SetOpacity(float opacity) {
  for (const auto& window : GetTransientTreeIterator(window_)) {
    window->SetOpacity(opacity);
  }
}

void ScopedTransformOverviewWindow::Close() {
  GetTransientRoot(window_)->CloseWidget();
}

void ScopedTransformOverviewWindow::PrepareForOverview() {
  DCHECK(!overview_started_);
  overview_started_ = true;
  window_->GetWindowState()->set_ignored_by_shelf(true);
  ShowWindowIfMinimized();
}

}  // namespace ash
