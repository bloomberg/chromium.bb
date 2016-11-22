// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/overview/scoped_transform_overview_window.h"

#include <algorithm>
#include <vector>

#include "ash/common/wm/overview/scoped_overview_animation_settings.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings_factory.h"
#include "ash/common/wm/overview/window_selector_item.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"

using WmWindows = std::vector<ash::WmWindow*>;

namespace ash {

namespace {

// When set to true by tests makes closing the widget synchronous.
bool immediate_close_for_tests = false;

// Delay closing window to allow it to shrink and fade out.
const int kCloseWindowDelayInMilliseconds = 150;

WmWindow* GetTransientRoot(WmWindow* window) {
  while (window && window->GetTransientParent())
    window = window->GetTransientParent();
  return window;
}

std::unique_ptr<ScopedOverviewAnimationSettings>
CreateScopedOverviewAnimationSettings(OverviewAnimationType animation_type,
                                      WmWindow* window) {
  return ScopedOverviewAnimationSettingsFactory::Get()
      ->CreateOverviewAnimationSettings(animation_type, window);
}

// An iterator class that traverses a WmWindow and all of its transient
// descendants.
class TransientDescendantIterator {
 public:
  // Creates an empty iterator.
  TransientDescendantIterator();

  // Copy constructor required for iterator purposes.
  TransientDescendantIterator(const TransientDescendantIterator& other) =
      default;

  // Iterates over |root_window| and all of its transient descendants.
  // Note |root_window| must not have a transient parent.
  explicit TransientDescendantIterator(WmWindow* root_window);

  // Prefix increment operator.  This assumes there are more items (i.e.
  // *this != TransientDescendantIterator()).
  const TransientDescendantIterator& operator++();

  // Comparison for STL-based loops.
  bool operator!=(const TransientDescendantIterator& other) const;

  // Dereference operator for STL-compatible iterators.
  WmWindow* operator*() const;

 private:
  // Explicit assignment operator defined because an explicit copy constructor
  // is needed and therefore the DISALLOW_COPY_AND_ASSIGN macro cannot be used.
  TransientDescendantIterator& operator=(
      const TransientDescendantIterator& other) = default;

  // The current window that |this| refers to. A null |current_window_| denotes
  // an empty iterator and is used as the last possible value in the traversal.
  WmWindow* current_window_;
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
    : current_window_(nullptr) {}

TransientDescendantIterator::TransientDescendantIterator(WmWindow* root_window)
    : current_window_(root_window) {
  DCHECK(!root_window->GetTransientParent());
}

// Performs a pre-order traversal of the transient descendants.
const TransientDescendantIterator& TransientDescendantIterator::operator++() {
  DCHECK(current_window_);

  const WmWindows transient_children = current_window_->GetTransientChildren();

  if (!transient_children.empty()) {
    current_window_ = transient_children.front();
  } else {
    while (current_window_) {
      WmWindow* parent = current_window_->GetTransientParent();
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

WmWindow* TransientDescendantIterator::operator*() const {
  return current_window_;
}

TransientDescendantIteratorRange::TransientDescendantIteratorRange(
    const TransientDescendantIterator& begin)
    : begin_(begin) {}

TransientDescendantIteratorRange GetTransientTreeIterator(WmWindow* window) {
  return TransientDescendantIteratorRange(
      TransientDescendantIterator(GetTransientRoot(window)));
}

}  // namespace

ScopedTransformOverviewWindow::ScopedTransformOverviewWindow(WmWindow* window)
    : window_(window),
      determined_original_window_shape_(false),
      ignored_by_shelf_(window->GetWindowState()->ignored_by_shelf()),
      overview_started_(false),
      original_transform_(window->GetTargetTransform()),
      original_opacity_(window->GetTargetOpacity()),
      weak_ptr_factory_(this) {}

ScopedTransformOverviewWindow::~ScopedTransformOverviewWindow() {}

void ScopedTransformOverviewWindow::RestoreWindow() {
  ShowHeader();
  if (minimized_widget_) {
    // TODO(oshima): Use unminimize animation instead of hiding animation.
    minimized_widget_->CloseNow();
    minimized_widget_.reset();
    return;
  }
  ScopedAnimationSettings animation_settings_list;
  BeginScopedAnimation(OverviewAnimationType::OVERVIEW_ANIMATION_RESTORE_WINDOW,
                       &animation_settings_list);
  SetTransform(window()->GetRootWindow(), original_transform_);
  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings =
      CreateScopedOverviewAnimationSettings(
          OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS,
          window_);
  window_->GetWindowState()->set_ignored_by_shelf(ignored_by_shelf_);
  SetOpacity(original_opacity_);
}

void ScopedTransformOverviewWindow::BeginScopedAnimation(
    OverviewAnimationType animation_type,
    ScopedAnimationSettings* animation_settings) {
  for (auto* window : GetTransientTreeIterator(GetOverviewWindow())) {
    animation_settings->push_back(
        CreateScopedOverviewAnimationSettings(animation_type, window));
  }
}

bool ScopedTransformOverviewWindow::Contains(const WmWindow* target) const {
  for (auto* window : GetTransientTreeIterator(window_)) {
    if (window->Contains(target))
      return true;
  }
  WmWindow* mirror = GetOverviewWindowForMinimizedState();
  return mirror && mirror->Contains(target);
}

gfx::Rect ScopedTransformOverviewWindow::GetTargetBoundsInScreen() const {
  gfx::Rect bounds;
  WmWindow* overview_window = GetOverviewWindow();
  for (auto* window : GetTransientTreeIterator(overview_window)) {
    // Ignore other window types when computing bounding box of window
    // selector target item.
    if (window != overview_window &&
        window->GetType() != ui::wm::WINDOW_TYPE_NORMAL &&
        window->GetType() != ui::wm::WINDOW_TYPE_PANEL) {
      continue;
    }
    bounds.Union(
        window->GetParent()->ConvertRectToScreen(window->GetTargetBounds()));
  }
  return bounds;
}

gfx::Rect ScopedTransformOverviewWindow::GetTransformedBounds() const {
  const int top_inset = GetTopInset();
  gfx::Rect bounds;
  WmWindow* overview_window = GetOverviewWindow();
  for (auto* window : GetTransientTreeIterator(overview_window)) {
    // Ignore other window types when computing bounding box of window
    // selector target item.
    if (window != overview_window &&
        (window->GetType() != ui::wm::WINDOW_TYPE_NORMAL &&
         window->GetType() != ui::wm::WINDOW_TYPE_PANEL)) {
      continue;
    }
    gfx::RectF window_bounds(window->GetTargetBounds());
    gfx::Transform new_transform =
        TransformAboutPivot(gfx::Point(window_bounds.x(), window_bounds.y()),
                            window->GetTargetTransform());
    new_transform.TransformRect(&window_bounds);

    // The preview title is shown above the preview window. Hide the window
    // header for apps or browser windows with no tabs (web apps) to avoid
    // showing both the window header and the preview title.
    if (top_inset > 0) {
      gfx::RectF header_bounds(window_bounds);
      header_bounds.set_height(top_inset);
      new_transform.TransformRect(&header_bounds);
      window_bounds.Inset(0, gfx::ToCeiledInt(header_bounds.height()), 0, 0);
    }
    bounds.Union(window->GetParent()->ConvertRectToScreen(
        ToEnclosingRect(window_bounds)));
  }
  return bounds;
}

SkColor ScopedTransformOverviewWindow::GetTopColor() const {
  for (auto* window : GetTransientTreeIterator(window_)) {
    // If there are regular windows in the transient ancestor tree, all those
    // windows are shown in the same overview item and the header is not masked.
    if (window != window_ && (window->GetType() == ui::wm::WINDOW_TYPE_NORMAL ||
                              window->GetType() == ui::wm::WINDOW_TYPE_PANEL)) {
      return SK_ColorTRANSPARENT;
    }
  }
  return window_->GetColorProperty(WmWindowProperty::TOP_VIEW_COLOR);
}

int ScopedTransformOverviewWindow::GetTopInset() const {
  // Mirror window doesn't have insets.
  if (minimized_widget_)
    return 0;
  for (auto* window : GetTransientTreeIterator(window_)) {
    // If there are regular windows in the transient ancestor tree, all those
    // windows are shown in the same overview item and the header is not masked.
    if (window != window_ && (window->GetType() == ui::wm::WINDOW_TYPE_NORMAL ||
                              window->GetType() == ui::wm::WINDOW_TYPE_PANEL)) {
      return 0;
    }
  }
  return window_->GetIntProperty(WmWindowProperty::TOP_VIEW_INSET);
}

void ScopedTransformOverviewWindow::OnWindowDestroyed() {
  window_ = nullptr;
}

float ScopedTransformOverviewWindow::GetItemScale(const gfx::Size& source,
                                                  const gfx::Size& target,
                                                  int top_view_inset,
                                                  int title_height) {
  return std::min(2.0f, static_cast<float>((target.height() - title_height)) /
                            (source.height() - top_view_inset));
}

gfx::Rect ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
    const gfx::Rect& rect,
    const gfx::Rect& bounds,
    int top_view_inset,
    int title_height) {
  DCHECK(!rect.IsEmpty());
  DCHECK_LE(top_view_inset, rect.height());
  const float scale =
      GetItemScale(rect.size(), bounds.size(), top_view_inset, title_height);
  const int horizontal_offset = gfx::ToFlooredInt(
      0.5 * (bounds.width() - gfx::ToFlooredInt(scale * rect.width())));
  const int width = bounds.width() - 2 * horizontal_offset;
  const int vertical_offset =
      title_height - gfx::ToCeiledInt(scale * top_view_inset);
  const int height = std::min(gfx::ToCeiledInt(scale * rect.height()),
                              bounds.height() - vertical_offset);
  return gfx::Rect(bounds.x() + horizontal_offset, bounds.y() + vertical_offset,
                   width, height);
}

gfx::Transform ScopedTransformOverviewWindow::GetTransformForRect(
    const gfx::Rect& src_rect,
    const gfx::Rect& dst_rect) {
  DCHECK(!src_rect.IsEmpty());
  gfx::Transform transform;
  transform.Translate(dst_rect.x() - src_rect.x(), dst_rect.y() - src_rect.y());
  transform.Scale(static_cast<float>(dst_rect.width()) / src_rect.width(),
                  static_cast<float>(dst_rect.height()) / src_rect.height());
  return transform;
}

void ScopedTransformOverviewWindow::SetTransform(
    WmWindow* root_window,
    const gfx::Transform& transform) {
  DCHECK(overview_started_);

  if (&transform != &original_transform_ &&
      !determined_original_window_shape_) {
    determined_original_window_shape_ = true;
    SkRegion* window_shape = window()->GetLayer()->alpha_shape();
    if (!original_window_shape_ && window_shape)
      original_window_shape_.reset(new SkRegion(*window_shape));
  }

  gfx::Point target_origin(GetTargetBoundsInScreen().origin());
  for (auto* window : GetTransientTreeIterator(GetOverviewWindow())) {
    WmWindow* parent_window = window->GetParent();
    gfx::Point original_origin =
        parent_window->ConvertRectToScreen(window->GetTargetBounds()).origin();
    gfx::Transform new_transform =
        TransformAboutPivot(gfx::Point(target_origin.x() - original_origin.x(),
                                       target_origin.y() - original_origin.y()),
                            transform);
    window->SetTransform(new_transform);
  }
}

void ScopedTransformOverviewWindow::SetOpacity(float opacity) {
  for (auto* window : GetTransientTreeIterator(GetOverviewWindow())) {
    window->SetOpacity(opacity);
  }
}

void ScopedTransformOverviewWindow::HideHeader() {
  // Mirrored Window does not have a header.
  if (minimized_widget_)
    return;
  gfx::Rect bounds(GetTargetBoundsInScreen().size());
  const int inset = GetTopInset();
  if (inset > 0) {
    // Use alpha shape to hide the window header.
    bounds.Inset(0, inset, 0, 0);
    std::unique_ptr<SkRegion> region(new SkRegion);
    region->setRect(RectToSkIRect(bounds));
    if (original_window_shape_)
      region->op(*original_window_shape_, SkRegion::kIntersect_Op);
    WmWindow* window = GetOverviewWindow();
    window->GetLayer()->SetAlphaShape(std::move(region));
    window->SetMasksToBounds(true);
  }
}

void ScopedTransformOverviewWindow::ShowHeader() {
  ui::Layer* layer = window()->GetLayer();
  if (original_window_shape_) {
    layer->SetAlphaShape(
        base::MakeUnique<SkRegion>(*original_window_shape_.get()));
  } else {
    layer->SetAlphaShape(nullptr);
  }
  window()->SetMasksToBounds(false);
}

void ScopedTransformOverviewWindow::UpdateMirrorWindowForMinimizedState() {
  // TODO(oshima): Disable animation.
  if (window_->GetShowState() == ui::SHOW_STATE_MINIMIZED) {
    if (!minimized_widget_)
      CreateMirrorWindowForMinimizedState();
  } else {
    minimized_widget_->CloseNow();
    minimized_widget_.reset();
  }
}

void ScopedTransformOverviewWindow::Close() {
  if (immediate_close_for_tests) {
    CloseWidget();
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&ScopedTransformOverviewWindow::CloseWidget,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kCloseWindowDelayInMilliseconds));
}

void ScopedTransformOverviewWindow::PrepareForOverview() {
  DCHECK(!overview_started_);
  overview_started_ = true;
  window_->GetWindowState()->set_ignored_by_shelf(true);
  if (window_->GetShowState() == ui::SHOW_STATE_MINIMIZED)
    CreateMirrorWindowForMinimizedState();
}

void ScopedTransformOverviewWindow::CloseWidget() {
  WmWindow* parent_window = GetTransientRoot(window_);
  if (parent_window)
    parent_window->CloseWidget();
}

// static
void ScopedTransformOverviewWindow::SetImmediateCloseForTests() {
  immediate_close_for_tests = true;
}

WmWindow* ScopedTransformOverviewWindow::GetOverviewWindow() const {
  if (minimized_widget_)
    return GetOverviewWindowForMinimizedState();
  return window_;
}

void ScopedTransformOverviewWindow::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    window_->Show();
    window_->Activate();
  }
}

void ScopedTransformOverviewWindow::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED && event->IsOnlyLeftMouseButton()) {
    window_->Show();
    window_->Activate();
  }
}

WmWindow* ScopedTransformOverviewWindow::GetOverviewWindowForMinimizedState()
    const {
  return minimized_widget_
             ? WmLookup::Get()->GetWindowForWidget(minimized_widget_.get())
             : nullptr;
}

void ScopedTransformOverviewWindow::CreateMirrorWindowForMinimizedState() {
  DCHECK(!minimized_widget_.get());
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.visible_on_all_workspaces = true;
  params.name = "OverviewModeMinimized";
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.accept_events = true;
  minimized_widget_.reset(new views::Widget);
  window_->GetRootWindow()
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          minimized_widget_.get(), window_->GetParent()->GetShellWindowId(),
          &params);
  minimized_widget_->set_focus_on_creation(false);
  minimized_widget_->Init(params);

  views::View* mirror_view = window_->CreateViewWithRecreatedLayers().release();
  mirror_view->SetVisible(true);
  mirror_view->SetTargetHandler(this);
  minimized_widget_->SetContentsView(mirror_view);
  gfx::Rect bounds(window_->GetBoundsInScreen());
  gfx::Size preferred = mirror_view->GetPreferredSize();
  // In unit tests, the content view can have empty size.
  if (!preferred.IsEmpty()) {
    int inset = bounds.height() - preferred.height();
    bounds.Inset(0, 0, 0, inset);
  }
  minimized_widget_->SetBounds(bounds);
  minimized_widget_->Show();
}

}  // namespace ash
