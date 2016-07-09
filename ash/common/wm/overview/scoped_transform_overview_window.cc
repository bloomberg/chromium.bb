// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/overview/scoped_transform_overview_window.h"

#include <algorithm>
#include <vector>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings_factory.h"
#include "ash/common/wm/overview/window_selector_item.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform_util.h"

using WmWindows = std::vector<ash::WmWindow*>;

namespace ash {

namespace {

// When set to true by tests makes closing the widget synchronous.
bool immediate_close_for_tests = false;

// The opacity level that windows will be set to when they are restored.
const float kRestoreWindowOpacity = 1.0f;

// Alpha value used to paint mask layer that masks the original window header.
const int kOverviewContentMaskAlpha = 255;

// Delay closing window with Material Design to allow it to shrink and fade out.
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

// Mask layer that clips the window's original header in overview mode.
// Only used with Material Design.
class ScopedTransformOverviewWindow::OverviewContentMask
    : public ui::LayerDelegate {
 public:
  explicit OverviewContentMask(float radius);
  ~OverviewContentMask() override;

  ui::Layer* layer() { return &layer_; }

  // Overridden from LayerDelegate.
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  base::Closure PrepareForLayerBoundsChange() override;

 private:
  ui::Layer layer_;
  float radius_;

  DISALLOW_COPY_AND_ASSIGN(OverviewContentMask);
};

ScopedTransformOverviewWindow::OverviewContentMask::OverviewContentMask(
    float radius)
    : layer_(ui::LAYER_TEXTURED), radius_(radius) {
  layer_.set_delegate(this);
}

ScopedTransformOverviewWindow::OverviewContentMask::~OverviewContentMask() {
  layer_.set_delegate(nullptr);
}

void ScopedTransformOverviewWindow::OverviewContentMask::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Rect bounds(layer()->bounds().size());

  // Tile a window into an area, rounding the bottom corners.
  const SkRect rect = gfx::RectToSkRect(bounds);
  const SkScalar corner_radius_scalar = SkIntToScalar(radius_);
  SkScalar radii[8] = {0,
                       0,  // top-left
                       0,
                       0,  // top-right
                       corner_radius_scalar,
                       corner_radius_scalar,  // bottom-right
                       corner_radius_scalar,
                       corner_radius_scalar};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);

  // Set a mask.
  SkPaint paint;
  paint.setAlpha(kOverviewContentMaskAlpha);
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAntiAlias(true);
  recorder.canvas()->DrawPath(path, paint);
}

void ScopedTransformOverviewWindow::OverviewContentMask::
    OnDeviceScaleFactorChanged(float device_scale_factor) {
  // Redrawing will take care of scale factor change.
}

base::Closure ScopedTransformOverviewWindow::OverviewContentMask::
    PrepareForLayerBoundsChange() {
  return base::Closure();
}

ScopedTransformOverviewWindow::ScopedTransformOverviewWindow(WmWindow* window)
    : window_(window),
      minimized_(window->GetShowState() == ui::SHOW_STATE_MINIMIZED),
      ignored_by_shelf_(window->GetWindowState()->ignored_by_shelf()),
      overview_started_(false),
      original_transform_(window->GetTargetTransform()),
      original_opacity_(window->GetTargetOpacity()),
      weak_ptr_factory_(this) {}

ScopedTransformOverviewWindow::~ScopedTransformOverviewWindow() {}

void ScopedTransformOverviewWindow::RestoreWindow() {
  ScopedAnimationSettings animation_settings_list;
  BeginScopedAnimation(OverviewAnimationType::OVERVIEW_ANIMATION_RESTORE_WINDOW,
                       &animation_settings_list);
  SetTransform(window()->GetRootWindow(), original_transform_, 0);

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

  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    ui::Layer* layer = window()->GetLayer();
    layer->SetMaskLayer(nullptr);
    mask_.reset();

    if (original_window_shape_) {
      layer->SetAlphaShape(
          base::WrapUnique(new SkRegion(*original_window_shape_.get())));
    } else {
      layer->SetAlphaShape(nullptr);
    }
    window()->SetMasksToBounds(false);
  }
}

void ScopedTransformOverviewWindow::BeginScopedAnimation(
    OverviewAnimationType animation_type,
    ScopedAnimationSettings* animation_settings) {
  for (auto* window : GetTransientTreeIterator(window_)) {
    animation_settings->push_back(
        CreateScopedOverviewAnimationSettings(animation_type, window));
  }
}

bool ScopedTransformOverviewWindow::Contains(const WmWindow* target) const {
  for (auto* window : GetTransientTreeIterator(window_)) {
    if (window->Contains(target))
      return true;
  }
  return false;
}

gfx::Rect ScopedTransformOverviewWindow::GetTargetBoundsInScreen() const {
  gfx::Rect bounds;
  for (auto* window : GetTransientTreeIterator(window_)) {
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

float ScopedTransformOverviewWindow::GetItemScale(const gfx::Size& source,
                                                  const gfx::Size& target,
                                                  int top_view_inset,
                                                  int title_height) {
  if (ash::MaterialDesignController::IsOverviewMaterial()) {
    return std::min(2.0f, static_cast<float>((target.height() - title_height)) /
                              (source.height() - top_view_inset));
  }
  return std::min(
      1.0f, std::min(static_cast<float>(target.width()) / source.width(),
                     static_cast<float>(target.height()) / source.height()));
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
  if (!ash::MaterialDesignController::IsOverviewMaterial()) {
    return gfx::Rect(
        bounds.x() + 0.5 * (bounds.width() - scale * rect.width()),
        bounds.y() + title_height - scale * top_view_inset +
            0.5 * (bounds.height() -
                   (title_height + scale * (rect.height() - top_view_inset))),
        rect.width() * scale, rect.height() * scale);
  }
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
    const gfx::Transform& transform,
    float radius) {
  DCHECK(overview_started_);

  if (ash::MaterialDesignController::IsOverviewMaterial() &&
      &transform != &original_transform_) {
    gfx::Rect bounds(GetTargetBoundsInScreen().size());
    mask_.reset(new OverviewContentMask(radius));
    mask_->layer()->SetFillsBoundsOpaquely(false);
    mask_->layer()->SetBounds(bounds);
    window()->GetLayer()->SetMaskLayer(mask_->layer());

    SkRegion* window_shape = window()->GetLayer()->alpha_shape();
    if (!original_window_shape_ && window_shape)
      original_window_shape_.reset(new SkRegion(*window_shape));
    const int inset =
        window()->GetIntProperty(WmWindowProperty::TOP_VIEW_INSET);
    if (inset > 0) {
      bounds.Inset(0, inset, 0, 0);
      SkRegion* region = new SkRegion;
      region->setRect(RectToSkIRect(bounds));
      if (original_window_shape_)
        region->op(*original_window_shape_, SkRegion::kIntersect_Op);
      window()->GetLayer()->SetAlphaShape(base::WrapUnique(region));
      window()->SetMasksToBounds(true);
    }
  }

  gfx::Point target_origin(GetTargetBoundsInScreen().origin());

  for (auto* window : GetTransientTreeIterator(window_)) {
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
  for (auto* window : GetTransientTreeIterator(window_)) {
    window->SetOpacity(opacity);
  }
}

void ScopedTransformOverviewWindow::Close() {
  if (immediate_close_for_tests ||
      !ash::MaterialDesignController::IsOverviewMaterial()) {
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
  ShowWindowIfMinimized();
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

}  // namespace ash
