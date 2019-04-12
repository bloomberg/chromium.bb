// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_transform_window.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/overview/delayed_animation_observer_impl.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_observer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// When set to true by tests makes closing the widget synchronous.
bool immediate_close_for_tests = false;

// Delay closing window to allow it to shrink and fade out.
constexpr int kCloseWindowDelayInMilliseconds = 150;

ScopedOverviewTransformWindow::GridWindowFillMode GetWindowDimensionsType(
    aura::Window* window) {
  if (window->bounds().width() >
      window->bounds().height() *
          ScopedOverviewTransformWindow::kExtremeWindowRatioThreshold) {
    return ScopedOverviewTransformWindow::GridWindowFillMode::kLetterBoxed;
  }

  if (window->bounds().height() >
      window->bounds().width() *
          ScopedOverviewTransformWindow::kExtremeWindowRatioThreshold) {
    return ScopedOverviewTransformWindow::GridWindowFillMode::kPillarBoxed;
  }

  return ScopedOverviewTransformWindow::GridWindowFillMode::kNormal;
}

}  // namespace

class ScopedOverviewTransformWindow::LayerCachingAndFilteringObserver
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

// WindowMask is applied to overview windows to give them rounded edges while
// they are in overview mode.
class ScopedOverviewTransformWindow::WindowMask : public ui::LayerDelegate,
                                                  public aura::WindowObserver {
 public:
  explicit WindowMask(aura::Window* window)
      : layer_(ui::LAYER_TEXTURED), window_(window) {
    window_->AddObserver(this);
    layer_.set_delegate(this);
    layer_.SetFillsBoundsOpaquely(false);
  }

  ~WindowMask() override {
    if (window_)
      window_->RemoveObserver(this);
    layer_.set_delegate(nullptr);
  }

  void set_top_inset(int top_inset) { top_inset_ = top_inset; }
  ui::Layer* layer() { return &layer_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    cc::PaintFlags flags;
    flags.setAlpha(255);
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);

    // The amount of round applied on the mask gets scaled as |window_| gets
    // transformed, so reverse the transform so the final scaled round matches
    // |kOverviewWindowRoundingDp|.
    const gfx::Vector2dF scale = window_->transform().Scale2d();
    const SkScalar r_x =
        SkIntToScalar(std::round(kOverviewWindowRoundingDp / scale.x()));
    const SkScalar r_y =
        SkIntToScalar(std::round(kOverviewWindowRoundingDp / scale.y()));

    SkPath path;
    SkScalar radii[8] = {r_x, r_y, r_x, r_y, r_x, r_y, r_x, r_y};
    gfx::Rect bounds(layer()->size());
    bounds.Inset(0, top_inset_, 0, 0);
    path.addRoundRect(gfx::RectToSkRect(bounds), radii);

    ui::PaintRecorder recorder(context, layer()->size());
    recorder.canvas()->DrawPath(path, flags);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    layer_.SetBounds(new_bounds);
  }

  void OnWindowDestroying(aura::Window* window) override {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  ui::Layer layer_;
  int top_inset_ = 0;
  // Pointer to the window of which this is a mask to.
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(WindowMask);
};

ScopedOverviewTransformWindow::ScopedOverviewTransformWindow(
    OverviewItem* overview_item,
    aura::Window* window)
    : overview_item_(overview_item),
      window_(window),
      original_opacity_(window->layer()->GetTargetOpacity()),
      original_mask_layer_(window_->layer()->layer_mask_layer()),
      weak_ptr_factory_(this) {
  type_ = GetWindowDimensionsType(window);
  original_event_targeting_policy_ = window_->event_targeting_policy();
  window_->SetEventTargetingPolicy(ws::mojom::EventTargetingPolicy::NONE);
  window_->SetProperty(kIsShowingInOverviewKey, true);

  // Hide transient children which have been specified to be hidden in overview
  // mode.
  std::vector<aura::Window*> transient_children_to_hide;
  for (auto* transient : wm::GetTransientTreeIterator(window)) {
    if (transient == window)
      continue;

    if (transient->GetProperty(kHideInOverviewKey))
      transient_children_to_hide.push_back(transient);
  }

  if (!transient_children_to_hide.empty()) {
    hidden_transient_children_ = std::make_unique<ScopedOverviewHideWindows>(
        std::move(transient_children_to_hide), /*forced_hidden=*/true);
  }
}

ScopedOverviewTransformWindow::~ScopedOverviewTransformWindow() {
  window_->ClearProperty(ash::kIsShowingInOverviewKey);
  window_->SetEventTargetingPolicy(original_event_targeting_policy_);
  UpdateMask(/*show=*/false);
  StopObservingImplicitAnimations();
}

// static
float ScopedOverviewTransformWindow::GetItemScale(const gfx::SizeF& source,
                                                  const gfx::SizeF& target,
                                                  int top_view_inset,
                                                  int title_height) {
  return std::min(2.0f, (target.height() - title_height) /
                            (source.height() - top_view_inset));
}

// static
gfx::Transform ScopedOverviewTransformWindow::GetTransformForRect(
    const gfx::RectF& src_rect,
    const gfx::RectF& dst_rect) {
  DCHECK(!src_rect.IsEmpty());
  gfx::Transform transform;
  transform.Translate(dst_rect.x() - src_rect.x(), dst_rect.y() - src_rect.y());
  transform.Scale(dst_rect.width() / src_rect.width(),
                  dst_rect.height() / src_rect.height());
  return transform;
}

void ScopedOverviewTransformWindow::RestoreWindow(
    bool reset_transform,
    OverviewSession::EnterExitOverviewType type) {
  // Shadow controller may be null on shutdown.
  if (Shell::Get()->shadow_controller())
    Shell::Get()->shadow_controller()->UpdateShadowForWindow(window_);
  if (minimized_widget_) {
    // Fade out the minimized widget. This animation continues past the
    // lifetime of |this|.
    FadeOutWidgetAndMaybeSlideOnExit(
        std::move(minimized_widget_),
        GetExitOverviewAnimationTypeForMinimizedWindow(type));
    return;
  }

  if (reset_transform) {
    ScopedAnimationSettings animation_settings_list;
    BeginScopedAnimation(overview_item_->GetExitTransformAnimationType(),
                         &animation_settings_list);
    for (auto& settings : animation_settings_list) {
      auto exit_observer = std::make_unique<ExitAnimationObserver>();
      settings->AddObserver(exit_observer.get());
      Shell::Get()->overview_controller()->AddExitAnimationObserver(
          std::move(exit_observer));
    }

    // Use identity transform directly to reset window's transform when exiting
    // overview.
    SetTransform(GetOverviewWindow(), gfx::Transform());
    // Add requests to cache render surface and perform trilinear filtering for
    // the exit animation of overview mode. The requests will be removed when
    // the exit animation finishes.
    if (features::IsTrilinearFilteringEnabled()) {
      for (auto& settings : animation_settings_list) {
        settings->CacheRenderSurface();
        settings->TrilinearFiltering();
      }
    }
  }

  ScopedOverviewAnimationSettings animation_settings(
      overview_item_->GetExitOverviewAnimationType(), window_);
  SetOpacity(original_opacity_);
  window_->layer()->SetMaskLayer(original_mask_layer_);
}

void ScopedOverviewTransformWindow::BeginScopedAnimation(
    OverviewAnimationType animation_type,
    ScopedAnimationSettings* animation_settings) {
  if (animation_type == OVERVIEW_ANIMATION_NONE)
    return;

  for (auto* window : GetVisibleTransientTreeIterator(GetOverviewWindow())) {
    auto settings = std::make_unique<ScopedOverviewAnimationSettings>(
        animation_type, window);
    settings->DeferPaint();

    // Create an EnterAnimationObserver if this is an enter overview layout
    // animation.
    if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER) {
      auto enter_observer = std::make_unique<EnterAnimationObserver>();
      settings->AddObserver(enter_observer.get());
      Shell::Get()->overview_controller()->AddEnterAnimationObserver(
          std::move(enter_observer));
    }

    animation_settings->push_back(std::move(settings));
  }

  if (animation_type == OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW &&
      animation_settings->size() > 0u) {
    animation_settings->front()->AddObserver(this);
  }
}

bool ScopedOverviewTransformWindow::Contains(const aura::Window* target) const {
  for (auto* window : wm::GetTransientTreeIterator(window_)) {
    if (window->Contains(target))
      return true;
  }

  if (!minimized_widget_)
    return false;
  return minimized_widget_->GetNativeWindow()->Contains(target);
}

gfx::RectF ScopedOverviewTransformWindow::GetTransformedBounds() const {
  return ::ash::GetTransformedBounds(GetOverviewWindow(), GetTopInset());
}

int ScopedOverviewTransformWindow::GetTopInset() const {
  // Mirror window doesn't have insets.
  if (minimized_widget_)
    return 0;
  for (auto* window : GetVisibleTransientTreeIterator(window_)) {
    // If there are regular windows in the transient ancestor tree, all those
    // windows are shown in the same overview item and the header is not masked.
    if (window != window_ &&
        window->type() == aura::client::WINDOW_TYPE_NORMAL) {
      return 0;
    }
  }
  return window_->GetProperty(aura::client::kTopViewInset);
}

void ScopedOverviewTransformWindow::OnWindowDestroyed() {
  window_ = nullptr;
}

void ScopedOverviewTransformWindow::SetOpacity(float opacity) {
  for (auto* window : GetVisibleTransientTreeIterator(GetOverviewWindow()))
    window->layer()->SetOpacity(opacity);
}

void ScopedOverviewTransformWindow::UpdateMirrorWindowForMinimizedState() {
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

gfx::RectF ScopedOverviewTransformWindow::ShrinkRectToFitPreservingAspectRatio(
    const gfx::RectF& rect,
    const gfx::RectF& bounds,
    int top_view_inset,
    int title_height) {
  DCHECK(!rect.IsEmpty());
  DCHECK_LE(top_view_inset, rect.height());
  const float scale =
      GetItemScale(rect.size(), bounds.size(), top_view_inset, title_height);
  const float horizontal_offset = 0.5 * (bounds.width() - scale * rect.width());
  const float width = bounds.width() - 2.f * horizontal_offset;
  const float vertical_offset = title_height - scale * top_view_inset;
  const float height =
      std::min(scale * rect.height(), bounds.height() - vertical_offset);
  gfx::RectF new_bounds(bounds.x() + horizontal_offset,
                        bounds.y() + vertical_offset, width, height);

  switch (type()) {
    case ScopedOverviewTransformWindow::GridWindowFillMode::kLetterBoxed:
    case ScopedOverviewTransformWindow::GridWindowFillMode::kPillarBoxed: {
      // Attempt to scale |rect| to fit |bounds|. Maintain the aspect ratio of
      // |rect|. Letter boxed windows' width will match |bounds|'s height and
      // pillar boxed windows' height will match |bounds|'s height.
      const bool is_pillar =
          type() ==
          ScopedOverviewTransformWindow::GridWindowFillMode::kPillarBoxed;
      gfx::RectF src = rect;
      new_bounds = bounds;
      src.Inset(0, top_view_inset, 0, 0);
      new_bounds.Inset(0, title_height, 0, 0);
      float scale = is_pillar ? new_bounds.height() / src.height()
                              : new_bounds.width() / src.width();
      gfx::SizeF size(is_pillar ? src.width() * scale : new_bounds.width(),
                      is_pillar ? new_bounds.height() : src.height() * scale);
      new_bounds.ClampToCenteredSize(size);

      // Extend |new_bounds| in the vertical direction to account for the header
      // that will be hidden.
      if (top_view_inset > 0)
        new_bounds.Inset(0, -(scale * top_view_inset), 0, 0);

      // Save the original bounds minus the title into |overview_bounds_|
      // so a larger backdrop can be drawn behind the window after.
      overview_bounds_ = bounds;
      overview_bounds_->Inset(0, title_height, 0, 0);
      break;
    }
    default:
      break;
  }

  return new_bounds;
}

void ScopedOverviewTransformWindow::Close() {
  if (immediate_close_for_tests) {
    CloseWidget();
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ScopedOverviewTransformWindow::CloseWidget,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kCloseWindowDelayInMilliseconds));
}

void ScopedOverviewTransformWindow::PrepareForOverview() {
  Shell::Get()->shadow_controller()->UpdateShadowForWindow(window_);

  DCHECK(!overview_started_);
  overview_started_ = true;
  if (window_->GetProperty(aura::client::kShowStateKey) ==
      ui::SHOW_STATE_MINIMIZED) {
    CreateMirrorWindowForMinimizedState();
  }

  // Add requests to cache render surface and perform trilinear filtering. The
  // requests will be removed in dtor. So the requests will be valid during the
  // enter animation and the whole time during overview mode. For the exit
  // animation of overview mode, we need to add those requests again.
  if (features::IsTrilinearFilteringEnabled()) {
    for (auto* window : GetVisibleTransientTreeIterator(GetOverviewWindow())) {
      cached_and_filtered_layer_observers_.push_back(
          std::make_unique<LayerCachingAndFilteringObserver>(window->layer()));
    }
  }
}

void ScopedOverviewTransformWindow::CloseWidget() {
  aura::Window* parent_window = ::wm::GetTransientRoot(window_);
  if (parent_window)
    wm::CloseWidgetForWindow(parent_window);
}

// static
void ScopedOverviewTransformWindow::SetImmediateCloseForTests() {
  immediate_close_for_tests = true;
}

aura::Window* ScopedOverviewTransformWindow::GetOverviewWindow() const {
  if (minimized_widget_)
    return minimized_widget_->GetNativeWindow();
  return window_;
}

void ScopedOverviewTransformWindow::EnsureVisible() {
  original_opacity_ = 1.f;
}

void ScopedOverviewTransformWindow::UpdateWindowDimensionsType() {
  type_ = GetWindowDimensionsType(window_);
  overview_bounds_.reset();
}

void ScopedOverviewTransformWindow::UpdateMask(bool show) {
  // Add the mask which gives the overview item rounded corners, and add the
  // shadow around the window.
  ui::Layer* layer = minimized_widget_
                         ? minimized_widget_->GetNativeWindow()->layer()
                         : window_->layer();
  if (ash::features::ShouldUseShaderRoundedCorner()) {
    const float scale = layer->transform().Scale2d().x();
    static constexpr std::array<uint32_t, 4> kEmptyRadii = {0, 0, 0, 0};
    const std::array<uint32_t, 4> kRadii = {
        kOverviewWindowRoundingDp / scale, kOverviewWindowRoundingDp / scale,
        kOverviewWindowRoundingDp / scale, kOverviewWindowRoundingDp / scale};
    layer->SetRoundedCornerRadius(show ? kRadii : kEmptyRadii);
    layer->SetIsFastRoundedCorner(true);
    return;
  }
  if (!base::FeatureList::IsEnabled(features::kEnableOverviewRoundedCorners) ||
      !show) {
    mask_.reset();
    return;
  }
  mask_ = std::make_unique<WindowMask>(GetOverviewWindow());
  mask_->layer()->SetBounds(layer->bounds());
  mask_->set_top_inset(GetTopInset());
  layer->SetMaskLayer(mask_->layer());
}

void ScopedOverviewTransformWindow::CancelAnimationsListener() {
  StopObservingImplicitAnimations();
}

void ScopedOverviewTransformWindow::ResizeMinimizedWidgetIfNeeded() {
  if (!minimized_widget_)
    return;

  gfx::Rect bounds(window_->GetBoundsInScreen());
  if (bounds.size() == window_->GetBoundsInScreen().size())
    return;

  auto* preview_view =
      static_cast<wm::WindowPreviewView*>(minimized_widget_->GetContentsView());
  if (preview_view) {
    preview_view->RecreatePreviews();
    bounds.Inset(0, 0, 0,
                 bounds.height() - preview_view->GetPreferredSize().height());
    minimized_widget_->SetBounds(bounds);
  }
}

void ScopedOverviewTransformWindow::UpdateMinimizedWidget() {
  if (!minimized_widget_)
    return;

  wm::WindowPreviewView* preview_view =
      new wm::WindowPreviewView(window_, /*trilinear_filtering_on_init=*/false);
  preview_view->SetVisible(true);
  minimized_widget_->SetContentsView(preview_view);
}

void ScopedOverviewTransformWindow::OnLayerAnimationStarted(
    ui::LayerAnimationSequence* sequence) {
  // Remove the mask before animating because masks affect animation
  // performance. The mask will be added back once the animation is completed.
  overview_item_->UpdateMaskAndShadow();
}

void ScopedOverviewTransformWindow::OnImplicitAnimationsCompleted() {
  overview_item_->UpdateMaskAndShadow();
  overview_item_->OnDragAnimationCompleted();
}

gfx::Rect ScopedOverviewTransformWindow::GetMaskBoundsForTesting() const {
  if (!mask_)
    return gfx::Rect();
  return mask_->layer()->bounds();
}

void ScopedOverviewTransformWindow::CreateMirrorWindowForMinimizedState() {
  DCHECK(!minimized_widget_.get());
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.visible_on_all_workspaces = true;
  params.name = "OverviewModeMinimized";
  params.activatable = views::Widget::InitParams::Activatable::ACTIVATABLE_NO;
  params.accept_events = false;
  params.parent = window_->parent();
  minimized_widget_ = std::make_unique<views::Widget>();
  minimized_widget_->set_focus_on_creation(false);
  minimized_widget_->Init(params);

  // Trilinear filtering will be applied on the |minimized_widget_| in
  // PrepareForOverview() and RestoreWindow().
  wm::WindowPreviewView* mirror_view =
      new wm::WindowPreviewView(window_, /*trilinear_filtering_on_init=*/false);
  mirror_view->SetVisible(true);
  minimized_widget_->SetContentsView(mirror_view);
  gfx::Rect bounds(window_->GetBoundsInScreen());
  gfx::Size preferred = mirror_view->GetPreferredSize();
  // In unit tests, the content view can have empty size.
  if (!preferred.IsEmpty()) {
    int inset = bounds.height() - preferred.height();
    bounds.Inset(0, 0, 0, inset);
  }
  minimized_widget_->SetBounds(bounds);
  minimized_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  minimized_widget_->Show();
  minimized_widget_->SetOpacity(0.f);

  // Stack the minimized window at the bottom since it is never transformed in
  // and only faded in, so it should always be underneath non minimized windows.
  window_->parent()->StackChildAtBottom(minimized_widget_->GetNativeWindow());

  FadeInWidgetAndMaybeSlideOnEnter(
      minimized_widget_.get(), OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
      /*slide=*/false);
}

OverviewAnimationType
ScopedOverviewTransformWindow::GetExitOverviewAnimationTypeForMinimizedWindow(
    OverviewSession::EnterExitOverviewType type) {
  // EnterExitOverviewType can only be set to kWindowMinimized in talbet mode.
  // Fade out the minimized window without animation if switch from tablet mode
  // to clamshell mode.
  if (type == OverviewSession::EnterExitOverviewType::kWindowsMinimized) {
    return Shell::Get()
                   ->tablet_mode_controller()
                   ->IsTabletModeWindowManagerEnabled()
               ? OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER
               : OVERVIEW_ANIMATION_NONE;
  }

  DCHECK(overview_item_);
  return overview_item_->should_animate_when_exiting()
             ? OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT
             : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

}  // namespace ash
