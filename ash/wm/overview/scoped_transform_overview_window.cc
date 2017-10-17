// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_transform_overview_window.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_mirror_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// When set to true by tests makes closing the widget synchronous.
bool immediate_close_for_tests = false;

// Delay closing window to allow it to shrink and fade out.
const int kCloseWindowDelayInMilliseconds = 150;

aura::Window* GetTransientRoot(aura::Window* window) {
  while (window && ::wm::GetTransientParent(window))
    window = ::wm::GetTransientParent(window);
  return window;
}

// An iterator class that traverses an aura::Window and all of its transient
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
  explicit TransientDescendantIterator(aura::Window* root_window);

  // Prefix increment operator.  This assumes there are more items (i.e.
  // *this != TransientDescendantIterator()).
  const TransientDescendantIterator& operator++();

  // Comparison for STL-based loops.
  bool operator!=(const TransientDescendantIterator& other) const;

  // Dereference operator for STL-compatible iterators.
  aura::Window* operator*() const;

 private:
  // Explicit assignment operator defined because an explicit copy constructor
  // is needed and therefore the DISALLOW_COPY_AND_ASSIGN macro cannot be used.
  TransientDescendantIterator& operator=(
      const TransientDescendantIterator& other) = default;

  // The current window that |this| refers to. A null |current_window_| denotes
  // an empty iterator and is used as the last possible value in the traversal.
  aura::Window* current_window_;
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

TransientDescendantIterator::TransientDescendantIterator(
    aura::Window* root_window)
    : current_window_(root_window) {
  DCHECK(!::wm::GetTransientParent(root_window));
}

// Performs a pre-order traversal of the transient descendants.
const TransientDescendantIterator& TransientDescendantIterator::operator++() {
  DCHECK(current_window_);

  const aura::Window::Windows transient_children =
      ::wm::GetTransientChildren(current_window_);

  if (!transient_children.empty()) {
    current_window_ = transient_children.front();
  } else {
    while (current_window_) {
      aura::Window* parent = ::wm::GetTransientParent(current_window_);
      if (!parent) {
        current_window_ = nullptr;
        break;
      }
      const aura::Window::Windows transient_siblings =
          ::wm::GetTransientChildren(parent);
      auto iter = std::find(transient_siblings.begin(),
                            transient_siblings.end(), current_window_);
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
    : begin_(begin) {}

TransientDescendantIteratorRange GetTransientTreeIterator(
    aura::Window* window) {
  return TransientDescendantIteratorRange(
      TransientDescendantIterator(GetTransientRoot(window)));
}

}  // namespace

class ScopedTransformOverviewWindow::LayerCachingAndFilteringObserver
    : public ui::LayerObserver {
 public:
  LayerCachingAndFilteringObserver(ui::Layer* layer) : layer_(layer) {
    layer_->AddObserver(this);
    layer_->AddCacheRenderSurfaceRequest();
    layer_->AddTrilinearFilteringRequest();
  }
  ~LayerCachingAndFilteringObserver() override {
    if (layer_) {
      layer_->RemoveTrilinearFilteringRequest();
      layer_->RemoveCacheRenderSurfaceRequest();
      layer_->RemoveObserver(this);
    }
  }

  // ui::LayerObserver overrides:
  void LayerDestroyed(ui::Layer* layer) override {
    layer_->RemoveObserver(this);
    layer_ = nullptr;
  }

 private:
  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(LayerCachingAndFilteringObserver);
};

ScopedTransformOverviewWindow::ScopedTransformOverviewWindow(
    WindowSelectorItem* selector_item,
    aura::Window* window)
    : selector_item_(selector_item),
      window_(window),
      determined_original_window_shape_(false),
      ignored_by_shelf_(wm::GetWindowState(window)->ignored_by_shelf()),
      overview_started_(false),
      original_transform_(window->layer()->GetTargetTransform()),
      original_opacity_(window->layer()->GetTargetOpacity()),
      weak_ptr_factory_(this) {}

ScopedTransformOverviewWindow::~ScopedTransformOverviewWindow() {}

void ScopedTransformOverviewWindow::RestoreWindow() {
  ShowHeader();
  wm::GetWindowState(window_)->set_ignored_by_shelf(ignored_by_shelf_);
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
  // Add requests to cache render surface and perform trilinear filtering for
  // the exit animation of overview mode. The requests will be removed when the
  // exit animation finishes.
  for (auto& settings : animation_settings_list) {
    settings->CacheRenderSurface();
    settings->TrilinearFiltering();
  }

  ScopedOverviewAnimationSettings animation_settings(
      OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS,
      window_);
  SetOpacity(original_opacity_);
}

void ScopedTransformOverviewWindow::BeginScopedAnimation(
    OverviewAnimationType animation_type,
    ScopedAnimationSettings* animation_settings) {
  for (auto* window : GetTransientTreeIterator(GetOverviewWindow())) {
    auto settings = std::make_unique<ScopedOverviewAnimationSettings>(
        animation_type, window);
    settings->DeferPaint();
    animation_settings->push_back(std::move(settings));
  }
}

bool ScopedTransformOverviewWindow::Contains(const aura::Window* target) const {
  for (auto* window : GetTransientTreeIterator(window_)) {
    if (window->Contains(target))
      return true;
  }
  aura::Window* mirror = GetOverviewWindowForMinimizedState();
  return mirror && mirror->Contains(target);
}

gfx::Rect ScopedTransformOverviewWindow::GetTargetBoundsInScreen() const {
  gfx::Rect bounds;
  aura::Window* overview_window = GetOverviewWindow();
  for (auto* window : GetTransientTreeIterator(overview_window)) {
    // Ignore other window types when computing bounding box of window
    // selector target item.
    if (window != overview_window &&
        window->type() != aura::client::WINDOW_TYPE_NORMAL &&
        window->type() != aura::client::WINDOW_TYPE_PANEL) {
      continue;
    }
    gfx::Rect target_bounds = window->GetTargetBounds();
    ::wm::ConvertRectToScreen(window->parent(), &target_bounds);
    bounds.Union(target_bounds);
  }
  return bounds;
}

gfx::Rect ScopedTransformOverviewWindow::GetTransformedBounds() const {
  const int top_inset = GetTopInset();
  gfx::Rect bounds;
  aura::Window* overview_window = GetOverviewWindow();
  for (auto* window : GetTransientTreeIterator(overview_window)) {
    // Ignore other window types when computing bounding box of window
    // selector target item.
    if (window != overview_window &&
        (window->type() != aura::client::WINDOW_TYPE_NORMAL &&
         window->type() != aura::client::WINDOW_TYPE_PANEL)) {
      continue;
    }
    gfx::RectF window_bounds(window->GetTargetBounds());
    gfx::Transform new_transform =
        TransformAboutPivot(gfx::Point(window_bounds.x(), window_bounds.y()),
                            window->layer()->GetTargetTransform());
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
    gfx::Rect enclosing_bounds = ToEnclosingRect(window_bounds);
    ::wm::ConvertRectToScreen(window->parent(), &enclosing_bounds);
    bounds.Union(enclosing_bounds);
  }
  return bounds;
}

SkColor ScopedTransformOverviewWindow::GetTopColor() const {
  for (auto* window : GetTransientTreeIterator(window_)) {
    // If there are regular windows in the transient ancestor tree, all those
    // windows are shown in the same overview item and the header is not masked.
    if (window != window_ &&
        (window->type() == aura::client::WINDOW_TYPE_NORMAL ||
         window->type() == aura::client::WINDOW_TYPE_PANEL)) {
      return SK_ColorTRANSPARENT;
    }
  }
  return window_->GetProperty(aura::client::kTopViewColor);
}

int ScopedTransformOverviewWindow::GetTopInset() const {
  // Mirror window doesn't have insets.
  if (minimized_widget_)
    return 0;
  for (auto* window : GetTransientTreeIterator(window_)) {
    // If there are regular windows in the transient ancestor tree, all those
    // windows are shown in the same overview item and the header is not masked.
    if (window != window_ &&
        (window->type() == aura::client::WINDOW_TYPE_NORMAL ||
         window->type() == aura::client::WINDOW_TYPE_PANEL)) {
      return 0;
    }
  }
  return window_->GetProperty(aura::client::kTopViewInset);
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
    aura::Window* root_window,
    const gfx::Transform& transform) {
  DCHECK(overview_started_);

  if (&transform != &original_transform_ &&
      !determined_original_window_shape_) {
    determined_original_window_shape_ = true;
    const ShapeRects* window_shape = window()->layer()->alpha_shape();
    if (!original_window_shape_ && window_shape)
      original_window_shape_ = std::make_unique<ShapeRects>(*window_shape);
  }

  gfx::Point target_origin(GetTargetBoundsInScreen().origin());
  for (auto* window : GetTransientTreeIterator(GetOverviewWindow())) {
    aura::Window* parent_window = window->parent();
    gfx::Rect original_bounds(window->GetTargetBounds());
    ::wm::ConvertRectToScreen(parent_window, &original_bounds);
    gfx::Transform new_transform =
        TransformAboutPivot(gfx::Point(target_origin.x() - original_bounds.x(),
                                       target_origin.y() - original_bounds.y()),
                            transform);
    window->SetTransform(new_transform);
  }
}

void ScopedTransformOverviewWindow::SetOpacity(float opacity) {
  for (auto* window : GetTransientTreeIterator(GetOverviewWindow()))
    window->layer()->SetOpacity(opacity);
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
    std::unique_ptr<ShapeRects> shape;
    if (original_window_shape_) {
      // When the |window| has a shape, use the new bounds to clip that shape.
      shape = std::make_unique<ShapeRects>(*original_window_shape_);
      for (auto& rect : *shape)
        rect.Intersect(bounds);
    } else {
      shape = std::make_unique<ShapeRects>();
      shape->push_back(bounds);
    }
    aura::Window* window = GetOverviewWindow();
    window->layer()->SetAlphaShape(std::move(shape));
    window->layer()->SetMasksToBounds(true);
  }
}

void ScopedTransformOverviewWindow::ShowHeader() {
  ui::Layer* layer = window()->layer();
  layer->SetAlphaShape(original_window_shape_ ? std::make_unique<ShapeRects>(
                                                    *original_window_shape_)
                                              : nullptr);
  layer->SetMasksToBounds(false);
}

void ScopedTransformOverviewWindow::UpdateMirrorWindowForMinimizedState() {
  // TODO(oshima): Disable animation.
  if (window_->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED) {
    if (!minimized_widget_)
      CreateMirrorWindowForMinimizedState();
  } else {
    // If the original window is no longer minimized, make sure it will be
    // visible when we restore it when selection mode ends.
    EnsureVisible();
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
  wm::GetWindowState(window_)->set_ignored_by_shelf(true);
  if (window_->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED) {
    CreateMirrorWindowForMinimizedState();
  }
  // Add requests to cache render surface and perform trilinear filtering. The
  // requests will be removed in dctor. So the requests will be valid during the
  // enter animation and the whole time during overview mode. For the exit
  // animation of overview mode, we need to add those requests again.
  for (auto* window : GetTransientTreeIterator(GetOverviewWindow())) {
    cached_and_filtered_layer_observers_.push_back(
        std::make_unique<LayerCachingAndFilteringObserver>(window->layer()));
  }
}

void ScopedTransformOverviewWindow::CloseWidget() {
  aura::Window* parent_window = GetTransientRoot(window_);
  if (parent_window)
    wm::CloseWidgetForWindow(parent_window);
}

// static
void ScopedTransformOverviewWindow::SetImmediateCloseForTests() {
  immediate_close_for_tests = true;
}

aura::Window* ScopedTransformOverviewWindow::GetOverviewWindow() const {
  if (minimized_widget_)
    return GetOverviewWindowForMinimizedState();
  return window_;
}

void ScopedTransformOverviewWindow::EnsureVisible() {
  original_opacity_ = 1.f;
}

void ScopedTransformOverviewWindow::OnGestureEvent(ui::GestureEvent* event) {
  if (minimized_widget_ && SplitViewController::ShouldAllowSplitView()) {
    gfx::Point location(event->location());
    ::wm::ConvertPointToScreen(minimized_widget_->GetNativeWindow(), &location);
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
      case ui::ET_GESTURE_TAP_DOWN:
        selector_item_->HandlePressEvent(location);
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        selector_item_->HandleDragEvent(location);
        break;
      case ui::ET_GESTURE_END:
        selector_item_->HandleReleaseEvent(location);
        break;
      default:
        break;
    }
    event->SetHandled();
  } else if (event->type() == ui::ET_GESTURE_TAP) {
    EnsureVisible();
    window_->Show();
    wm::ActivateWindow(window_);
  }
}

void ScopedTransformOverviewWindow::OnMouseEvent(ui::MouseEvent* event) {
  if (minimized_widget_ && SplitViewController::ShouldAllowSplitView()) {
    gfx::Point location(event->location());
    ::wm::ConvertPointToScreen(minimized_widget_->GetNativeWindow(), &location);
    switch (event->type()) {
      case ui::ET_MOUSE_PRESSED:
        selector_item_->HandlePressEvent(location);
        break;
      case ui::ET_MOUSE_DRAGGED:
        selector_item_->HandleDragEvent(location);
        break;
      case ui::ET_MOUSE_RELEASED:
        selector_item_->HandleReleaseEvent(location);
        break;
      default:
        break;
    }
    event->SetHandled();
  } else if (event->type() == ui::ET_MOUSE_PRESSED &&
             event->IsOnlyLeftMouseButton()) {
    EnsureVisible();
    window_->Show();
    wm::ActivateWindow(window_);
  }
}

aura::Window*
ScopedTransformOverviewWindow::GetOverviewWindowForMinimizedState() const {
  return minimized_widget_ ? minimized_widget_->GetNativeWindow() : nullptr;
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
  params.parent = window_->parent();
  minimized_widget_ = std::make_unique<views::Widget>();
  minimized_widget_->set_focus_on_creation(false);
  minimized_widget_->Init(params);

  views::View* mirror_view = new wm::WindowMirrorView(window_);
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
