// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_presenter_impl.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/presenter/app_list_presenter_delegate_factory.h"
#include "ash/shell.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_metrics.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/widget/widget.h"

namespace app_list {
namespace {

inline ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

class StateAnimationMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  StateAnimationMetricsReporter() = default;
  ~StateAnimationMetricsReporter() override = default;

  void Report(int value) override {
    UMA_HISTOGRAM_PERCENTAGE("Apps.StateTransition.AnimationSmoothness", value);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(StateAnimationMetricsReporter);
};

// Callback from the compositor when it presented a valid frame. Used to
// record UMA of input latency.
void DidPresentCompositorFrame(base::TimeTicks event_time_stamp,
                               bool is_showing,
                               base::TimeTicks present_time,
                               base::TimeDelta refresh,
                               uint32_t flags) {
  if (present_time.is_null() || event_time_stamp.is_null() ||
      present_time < event_time_stamp) {
    return;
  }
  const base::TimeDelta input_latency = present_time - event_time_stamp;
  if (is_showing) {
    UMA_HISTOGRAM_TIMES(kAppListShowInputLatencyHistogram, input_latency);
  } else {
    UMA_HISTOGRAM_TIMES(kAppListHideInputLatencyHistogram, input_latency);
  }
}

}  // namespace

AppListPresenterImpl::AppListPresenterImpl(
    std::unique_ptr<AppListPresenterDelegateFactory> factory,
    ash::AppListControllerImpl* controller)
    : factory_(std::move(factory)),
      controller_(controller),
      state_animation_metrics_reporter_(
          std::make_unique<StateAnimationMetricsReporter>()) {
  DCHECK(factory_);
}

AppListPresenterImpl::~AppListPresenterImpl() {
  Dismiss(base::TimeTicks());
  presenter_delegate_.reset();
  // Ensures app list view goes before the controller since pagination model
  // lives in the controller and app list view would access it on destruction.
  if (view_) {
    view_->GetAppsPaginationModel()->RemoveObserver(this);
    if (view_->GetWidget())
      view_->GetWidget()->CloseNow();
  }
}

aura::Window* AppListPresenterImpl::GetWindow() {
  return is_visible_ && view_ ? view_->GetWidget()->GetNativeWindow() : nullptr;
}

void AppListPresenterImpl::Show(int64_t display_id,
                                base::TimeTicks event_time_stamp) {
  if (is_visible_) {
    if (display_id != GetDisplayId())
      Dismiss(event_time_stamp);
    return;
  }

  is_visible_ = true;
  RequestPresentationTime(display_id, event_time_stamp);

  if (view_) {
    ScheduleAnimation();
  } else {
    presenter_delegate_ = factory_->GetDelegate(this);
    view_delegate_ = presenter_delegate_->GetViewDelegate();
    DCHECK(view_delegate_);
    // Note the AppListViewDelegate outlives the AppListView. For Ash, the view
    // is destroyed when dismissed.
    AppListView* view = new AppListView(view_delegate_);
    presenter_delegate_->Init(view, display_id, current_apps_page_);
    SetView(view);
  }
  presenter_delegate_->OnShown(display_id);
  view_delegate_->ViewShown(display_id);
  NotifyTargetVisibilityChanged(GetTargetVisibility());
  NotifyVisibilityChanged(GetTargetVisibility(), display_id);
}

void AppListPresenterImpl::Dismiss(base::TimeTicks event_time_stamp) {
  if (!is_visible_)
    return;

  // If the app list is currently visible, there should be an existing view.
  DCHECK(view_);

  is_visible_ = false;
  const int64_t display_id = GetDisplayId();
  RequestPresentationTime(display_id, event_time_stamp);
  // The dismissal may have occurred in response to the app list losing
  // activation. Otherwise, our widget is currently active. When the animation
  // completes we'll hide the widget, changing activation. If a menu is shown
  // before the animation completes then the activation change triggers the menu
  // to close. By deactivating now we ensure there is no activation change when
  // the animation completes and any menus stay open.
  if (view_->GetWidget()->IsActive())
    view_->GetWidget()->Deactivate();

  view_delegate_->ViewClosing();
  presenter_delegate_->OnDismissed();
  ScheduleAnimation();
  NotifyTargetVisibilityChanged(GetTargetVisibility());
  NotifyVisibilityChanged(GetTargetVisibility(), display_id);
  base::RecordAction(base::UserMetricsAction("Launcher_Dismiss"));
}

bool AppListPresenterImpl::Back() {
  if (!is_visible_)
    return false;

  // If the app list is currently visible, there should be an existing view.
  DCHECK(view_);

  return view_->app_list_main_view()->contents_view()->Back();
}

void AppListPresenterImpl::ToggleAppList(int64_t display_id,
                                         base::TimeTicks event_time_stamp) {
  if (IsVisible()) {
    Dismiss(event_time_stamp);
    return;
  }
  Show(display_id, event_time_stamp);
}

bool AppListPresenterImpl::IsVisible() const {
  return view_ && view_->GetWidget()->IsVisible();
}

bool AppListPresenterImpl::GetTargetVisibility() const {
  return is_visible_;
}

void AppListPresenterImpl::UpdateYPositionAndOpacity(int y_position_in_screen,
                                                     float background_opacity) {
  if (!is_visible_)
    return;

  if (view_)
    view_->UpdateYPositionAndOpacity(y_position_in_screen, background_opacity);
}

void AppListPresenterImpl::EndDragFromShelf(
    app_list::AppListViewState app_list_state) {
  if (view_) {
    if (app_list_state == AppListViewState::CLOSED ||
        view_->app_list_state() == AppListViewState::CLOSED) {
      view_->Dismiss();
    } else {
      view_->SetState(AppListViewState(app_list_state));
    }
    view_->SetIsInDrag(false);
    view_->DraggingLayout();
  }
}

void AppListPresenterImpl::ProcessMouseWheelOffset(int y_scroll_offset) {
  if (view_)
    view_->HandleScroll(y_scroll_offset, ui::ET_MOUSEWHEEL);
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, private:

void AppListPresenterImpl::SetView(AppListView* view) {
  DCHECK(view_ == nullptr);
  DCHECK(is_visible_);

  view_ = view;
  views::Widget* widget = view_->GetWidget();
  widget->AddObserver(this);
  aura::client::GetFocusClient(widget->GetNativeView())->AddObserver(this);
  view_->GetAppsPaginationModel()->AddObserver(this);

  // Sync the |onscreen_keyboard_shown_| in case |view_| is not initiated when
  // the on-screen is shown.
  view_->set_onscreen_keyboard_shown(controller_->onscreen_keyboard_shown());
  view_->ShowWhenReady();
}

void AppListPresenterImpl::ResetView() {
  if (!view_)
    return;

  views::Widget* widget = view_->GetWidget();
  widget->RemoveObserver(this);
  GetLayer(widget)->GetAnimator()->RemoveObserver(this);
  presenter_delegate_.reset();
  aura::client::GetFocusClient(widget->GetNativeView())->RemoveObserver(this);

  view_->GetAppsPaginationModel()->RemoveObserver(this);

  view_ = nullptr;
}

void AppListPresenterImpl::ScheduleAnimation() {
  // Stop observing previous animation.
  StopObservingImplicitAnimations();

  views::Widget* widget = view_->GetWidget();
  ui::Layer* layer = GetLayer(widget);
  layer->GetAnimator()->StopAnimating();
  aura::Window* root_window = widget->GetNativeView()->GetRootWindow();
  const gfx::Vector2d offset =
      presenter_delegate_->GetVisibilityAnimationOffset(root_window);
  base::TimeDelta animation_duration =
      presenter_delegate_->GetVisibilityAnimationDuration(root_window,
                                                          is_visible_);
  gfx::Rect target_bounds = widget->GetNativeView()->bounds();
  target_bounds.Offset(offset);
  widget->GetNativeView()->SetBounds(target_bounds);
  gfx::Transform transform;
  transform.Translate(-offset.x(), -offset.y());
  layer->SetTransform(transform);

  {
    ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
    animation.SetTransitionDuration(animation_duration);
    animation.SetAnimationMetricsReporter(
        state_animation_metrics_reporter_.get());
    animation.AddObserver(this);

    layer->SetTransform(gfx::Transform());
  }
  view_->StartCloseAnimation(animation_duration);
}

int64_t AppListPresenterImpl::GetDisplayId() {
  views::Widget* widget = view_ ? view_->GetWidget() : nullptr;
  if (!widget)
    return display::kInvalidDisplayId;
  return display::Screen::GetScreen()
      ->GetDisplayNearestView(widget->GetNativeView())
      .id();
}

void AppListPresenterImpl::NotifyVisibilityChanged(bool visible,
                                                   int64_t display_id) {
  // Skip adjacent same changes.
  if (last_visible_ == visible && last_display_id_ == display_id)
    return;
  last_visible_ = visible;
  last_display_id_ = display_id;

  if (controller_)
    controller_->OnVisibilityChanged(visible);

  // Notify the Shell and its observers of the app list visibility change.
  aura::Window* root = ash::Shell::Get()->GetRootWindowForDisplayId(display_id);
  ash::Shell::Get()->NotifyAppListVisibilityChanged(visible, root);
}

void AppListPresenterImpl::NotifyTargetVisibilityChanged(bool visible) {
  // Skip adjacent same changes.
  if (last_target_visible_ == visible)
    return;
  last_target_visible_ = visible;

  if (controller_)
    controller_->OnTargetVisibilityChanged(visible);
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl,  aura::client::FocusChangeObserver implementation:

void AppListPresenterImpl::OnWindowFocused(aura::Window* gained_focus,
                                           aura::Window* lost_focus) {
  if (view_ && is_visible_) {
    aura::Window* applist_window = view_->GetWidget()->GetNativeView();
    aura::Window* applist_container = applist_window->parent();
    if (applist_container->Contains(lost_focus) &&
        (!gained_focus || !applist_container->Contains(gained_focus)) &&
        !switches::ShouldNotDismissOnBlur()) {
      Dismiss(base::TimeTicks());
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, ui::ImplicitAnimationObserver implementation:

void AppListPresenterImpl::OnImplicitAnimationsCompleted() {
  if (is_visible_)
    view_->GetWidget()->Activate();
  else
    view_->GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, views::WidgetObserver implementation:

void AppListPresenterImpl::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(view_->GetWidget(), widget);
  if (is_visible_)
    Dismiss(base::TimeTicks());
  ResetView();
}

void AppListPresenterImpl::OnWidgetVisibilityChanged(views::Widget* widget,
                                                     bool visible) {
  DCHECK_EQ(view_->GetWidget(), widget);
  NotifyVisibilityChanged(visible, GetDisplayId());
}

////////////////////////////////////////////////////////////////////////////////
// AppListPresenterImpl, PaginationModelObserver implementation:

void AppListPresenterImpl::TotalPagesChanged() {}

void AppListPresenterImpl::SelectedPageChanged(int old_selected,
                                               int new_selected) {
  current_apps_page_ = new_selected;
}

void AppListPresenterImpl::TransitionStarted() {}

void AppListPresenterImpl::TransitionChanged() {}

void AppListPresenterImpl::TransitionEnded() {}

void AppListPresenterImpl::RequestPresentationTime(
    int64_t display_id,
    base::TimeTicks event_time_stamp) {
  if (event_time_stamp.is_null())
    return;
  aura::Window* root_window =
      ash::Shell::Get()->GetRootWindowForDisplayId(display_id);
  if (!root_window)
    return;
  ui::Compositor* compositor = root_window->layer()->GetCompositor();
  if (!compositor)
    return;
  compositor->RequestPresentationTimeForNextFrame(base::BindOnce(
      &DidPresentCompositorFrame, event_time_stamp, is_visible_));
}

}  // namespace app_list
