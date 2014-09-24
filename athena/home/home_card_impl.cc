// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/home_card_impl.h"

#include <cmath>
#include <limits>

#include "athena/env/public/athena_env.h"
#include "athena/home/app_list_view_delegate.h"
#include "athena/home/athena_start_page_view.h"
#include "athena/home/home_card_constants.h"
#include "athena/home/minimized_home.h"
#include "athena/home/public/app_model_builder.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/util/container_priorities.h"
#include "athena/wm/public/window_manager.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/public/activation_client.h"

namespace athena {
namespace {

HomeCard* instance = NULL;

gfx::Rect GetBoundsForState(const gfx::Rect& screen_bounds,
                            HomeCard::State state) {
  switch (state) {
    case HomeCard::HIDDEN:
      break;

    case HomeCard::VISIBLE_CENTERED:
      return screen_bounds;

    // Do not change the home_card's size, only changes the top position
    // instead, because size change causes unnecessary re-layouts.
    case HomeCard::VISIBLE_BOTTOM:
      return gfx::Rect(0,
                       screen_bounds.bottom() - kHomeCardHeight,
                       screen_bounds.width(),
                       screen_bounds.height());
    case HomeCard::VISIBLE_MINIMIZED:
      return gfx::Rect(0,
                       screen_bounds.bottom() - kHomeCardMinimizedHeight,
                       screen_bounds.width(),
                       screen_bounds.height());
  }

  NOTREACHED();
  return gfx::Rect();
}

}  // namespace

// Makes sure the homecard is center-aligned horizontally and bottom-aligned
// vertically.
class HomeCardLayoutManager : public aura::LayoutManager {
 public:
  HomeCardLayoutManager()
      : home_card_(NULL),
        minimized_layer_(NULL) {}

  virtual ~HomeCardLayoutManager() {}

  void Layout(bool animate, gfx::Tween::Type tween_type) {
    // |home_card| could be detached from the root window (e.g. when it is being
    // destroyed).
    if (!home_card_ || !home_card_->IsVisible() || !home_card_->GetRootWindow())
      return;

    scoped_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (animate) {
      settings.reset(new ui::ScopedLayerAnimationSettings(
          home_card_->layer()->GetAnimator()));
      settings->SetTweenType(tween_type);
    }
    SetChildBoundsDirect(home_card_, GetBoundsForState(
        home_card_->GetRootWindow()->bounds(), HomeCard::Get()->GetState()));
  }

  void SetMinimizedLayer(ui::Layer* minimized_layer) {
    minimized_layer_ = minimized_layer;
    UpdateMinimizedHomeBounds();
  }

 private:
  void UpdateMinimizedHomeBounds() {
    gfx::Rect minimized_bounds = minimized_layer_->parent()->bounds();
    minimized_bounds.set_y(
        minimized_bounds.bottom() - kHomeCardMinimizedHeight);
    minimized_bounds.set_height(kHomeCardMinimizedHeight);
    minimized_layer_->SetBounds(minimized_bounds);
  }

  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {
    Layout(false, gfx::Tween::LINEAR);
    UpdateMinimizedHomeBounds();
  }
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    if (!home_card_) {
      home_card_ = child;
      Layout(false, gfx::Tween::LINEAR);
    }
  }
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {
    if (home_card_ == child)
      home_card_ = NULL;
  }
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {
  }
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {
    if (home_card_ == child)
      Layout(false, gfx::Tween::LINEAR);
  }
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* home_card_;
  ui::Layer* minimized_layer_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardLayoutManager);
};

// The container view of home card contents of each state.
class HomeCardView : public views::WidgetDelegateView {
 public:
  HomeCardView(app_list::AppListViewDelegate* view_delegate,
               aura::Window* container,
               HomeCardGestureManager::Delegate* gesture_delegate)
      : gesture_delegate_(gesture_delegate) {
    SetLayoutManager(new views::FillLayout());
    // Ideally AppListMainView should be used here and have AthenaStartPageView
    // as its child view, so that custom pages and apps grid are available in
    // the home card.
    // TODO(mukai): make it so after the detailed UI has been fixed.
    main_view_ = new AthenaStartPageView(view_delegate);
    AddChildView(main_view_);
  }

  void SetStateProgress(HomeCard::State from_state,
                        HomeCard::State to_state,
                        float progress) {
    // TODO(mukai): not clear the focus, but simply close the virtual keyboard.
    GetFocusManager()->ClearFocus();
    if (from_state == HomeCard::VISIBLE_CENTERED)
      main_view_->SetLayoutState(1.0f - progress);
    else if (to_state == HomeCard::VISIBLE_CENTERED)
      main_view_->SetLayoutState(progress);
    UpdateShadow(true);
  }

  void SetStateWithAnimation(HomeCard::State state,
                             gfx::Tween::Type tween_type) {
    UpdateShadow(state != HomeCard::VISIBLE_MINIMIZED);
    if (state == HomeCard::VISIBLE_CENTERED)
      main_view_->RequestFocusOnSearchBox();
    else
      GetWidget()->GetFocusManager()->ClearFocus();

    main_view_->SetLayoutStateWithAnimation(
        (state == HomeCard::VISIBLE_CENTERED) ? 1.0f : 0.0f, tween_type);
  }

  void ClearGesture() {
    gesture_manager_.reset();
  }

  // views::View:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (!gesture_manager_ &&
        event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
      gesture_manager_.reset(new HomeCardGestureManager(
          gesture_delegate_,
          GetWidget()->GetNativeWindow()->GetRootWindow()->bounds()));
    }

    if (gesture_manager_)
      gesture_manager_->ProcessGestureEvent(event);
  }
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    if (HomeCard::Get()->GetState() == HomeCard::VISIBLE_MINIMIZED &&
        event.IsLeftMouseButton() && event.GetClickCount() == 1) {
      athena::WindowManager::Get()->ToggleOverview();
      return true;
    }
    return false;
  }

 private:
  void UpdateShadow(bool should_show) {
    wm::SetShadowType(
        GetWidget()->GetNativeWindow(),
        should_show ? wm::SHADOW_TYPE_RECTANGULAR : wm::SHADOW_TYPE_NONE);
  }

  // views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  AthenaStartPageView* main_view_;
  scoped_ptr<HomeCardGestureManager> gesture_manager_;
  HomeCardGestureManager::Delegate* gesture_delegate_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardView);
};

HomeCardImpl::HomeCardImpl(AppModelBuilder* model_builder)
    : model_builder_(model_builder),
      state_(HIDDEN),
      original_state_(VISIBLE_MINIMIZED),
      home_card_widget_(NULL),
      home_card_view_(NULL),
      layout_manager_(NULL),
      activation_client_(NULL) {
  DCHECK(!instance);
  instance = this;
  WindowManager::Get()->AddObserver(this);
}

HomeCardImpl::~HomeCardImpl() {
  DCHECK(instance);
  WindowManager::Get()->RemoveObserver(this);
  if (activation_client_)
    activation_client_->RemoveObserver(this);
  home_card_widget_->CloseNow();

  // Reset the view delegate first as it access search provider during
  // shutdown.
  view_delegate_.reset();
  search_provider_.reset();
  instance = NULL;
}

void HomeCardImpl::Init() {
  InstallAccelerators();
  ScreenManager::ContainerParams params("HomeCardContainer", CP_HOME_CARD);
  params.can_activate_children = true;
  aura::Window* container = ScreenManager::Get()->CreateContainer(params);
  layout_manager_ = new HomeCardLayoutManager();

  container->SetLayoutManager(layout_manager_);
  wm::SetChildWindowVisibilityChangesAnimated(container);

  view_delegate_.reset(new AppListViewDelegate(model_builder_.get()));
  if (search_provider_)
    view_delegate_->RegisterSearchProvider(search_provider_.get());

  home_card_view_ = new HomeCardView(view_delegate_.get(), container, this);
  home_card_widget_ = new views::Widget();
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.parent = container;
  widget_params.delegate = home_card_view_;
  widget_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  home_card_widget_->Init(widget_params);

  minimized_home_ = CreateMinimizedHome();
  container->layer()->Add(minimized_home_->layer());
  container->layer()->StackAtTop(minimized_home_->layer());
  layout_manager_->SetMinimizedLayer(minimized_home_->layer());

  SetState(VISIBLE_MINIMIZED);
  home_card_view_->Layout();

  activation_client_ =
      aura::client::GetActivationClient(container->GetRootWindow());
  if (activation_client_)
    activation_client_->AddObserver(this);

  AthenaEnv::Get()->SetDisplayWorkAreaInsets(
      gfx::Insets(0, 0, kHomeCardMinimizedHeight, 0));
}

aura::Window* HomeCardImpl::GetHomeCardWindowForTest() const {
  return home_card_widget_ ? home_card_widget_->GetNativeWindow() : NULL;
}

void HomeCardImpl::InstallAccelerators() {
  const AcceleratorData accelerator_data[] = {
      {TRIGGER_ON_PRESS, ui::VKEY_L, ui::EF_CONTROL_DOWN,
       COMMAND_SHOW_HOME_CARD, AF_NONE},
  };
  AcceleratorManager::Get()->RegisterAccelerators(
      accelerator_data, arraysize(accelerator_data), this);
}

void HomeCardImpl::SetState(HomeCard::State state) {
  if (state_ == state)
    return;

  // Update |state_| before changing the visibility of the widgets, so that
  // LayoutManager callbacks get the correct state.
  HomeCard::State old_state = state_;
  state_ = state;
  original_state_ = state;

  if (old_state == VISIBLE_MINIMIZED ||
      state_ == VISIBLE_MINIMIZED) {
    minimized_home_->layer()->SetVisible(true);
    {
      ui::ScopedLayerAnimationSettings settings(
          minimized_home_->layer()->GetAnimator());
      minimized_home_->layer()->SetVisible(state_ == VISIBLE_MINIMIZED);
      minimized_home_->layer()->SetOpacity(
          state_ == VISIBLE_MINIMIZED ? 1.0f : 0.0f);
    }
  }
  if (state_ == HIDDEN) {
    home_card_widget_->Hide();
  } else {
    if (state_ == VISIBLE_MINIMIZED)
      home_card_widget_->ShowInactive();
    else
      home_card_widget_->Show();
    home_card_view_->SetStateWithAnimation(state, gfx::Tween::EASE_IN_OUT);
    layout_manager_->Layout(true, gfx::Tween::EASE_IN_OUT);
  }
}

HomeCard::State HomeCardImpl::GetState() {
  return state_;
}

void HomeCardImpl::RegisterSearchProvider(
    app_list::SearchProvider* search_provider) {
  DCHECK(!search_provider_);
  search_provider_.reset(search_provider);
  view_delegate_->RegisterSearchProvider(search_provider_.get());
}

void HomeCardImpl::UpdateVirtualKeyboardBounds(
    const gfx::Rect& bounds) {
  if (state_ == VISIBLE_MINIMIZED && !bounds.IsEmpty()) {
    SetState(HIDDEN);
    original_state_ = VISIBLE_MINIMIZED;
  } else if (state_ == VISIBLE_BOTTOM && !bounds.IsEmpty()) {
    SetState(VISIBLE_CENTERED);
    original_state_ = VISIBLE_BOTTOM;
  } else if (state_ != original_state_ && bounds.IsEmpty()) {
    SetState(original_state_);
  }
}

bool HomeCardImpl::IsCommandEnabled(int command_id) const {
  return true;
}

bool HomeCardImpl::OnAcceleratorFired(int command_id,
                                      const ui::Accelerator& accelerator) {
  DCHECK_EQ(COMMAND_SHOW_HOME_CARD, command_id);

  if (state_ == VISIBLE_CENTERED && original_state_ != VISIBLE_BOTTOM)
    SetState(VISIBLE_MINIMIZED);
  else if (state_ == VISIBLE_MINIMIZED)
    SetState(VISIBLE_CENTERED);
  return true;
}

void HomeCardImpl::OnGestureEnded(State final_state, bool is_fling) {
  home_card_view_->ClearGesture();
  if (state_ != final_state &&
      (state_ == VISIBLE_MINIMIZED || final_state == VISIBLE_MINIMIZED)) {
    SetState(final_state);
    WindowManager::Get()->ToggleOverview();
  } else {
    state_ = final_state;
    // When the animation happens after a fling, EASE_IN_OUT would cause weird
    // slow-down right after the finger release because of slow-in. Therefore
    // EASE_OUT is better.
    gfx::Tween::Type tween_type =
        is_fling ? gfx::Tween::EASE_OUT : gfx::Tween::EASE_IN_OUT;
    home_card_view_->SetStateWithAnimation(state_, tween_type);
    layout_manager_->Layout(true, tween_type);
  }
}

void HomeCardImpl::OnGestureProgressed(
    State from_state, State to_state, float progress) {
  if (from_state == VISIBLE_MINIMIZED || to_state == VISIBLE_MINIMIZED) {
    minimized_home_->layer()->SetVisible(true);
    float opacity =
        (from_state == VISIBLE_MINIMIZED) ? 1.0f - progress : progress;
    minimized_home_->layer()->SetOpacity(opacity);
  }
  gfx::Rect screen_bounds =
      home_card_widget_->GetNativeWindow()->GetRootWindow()->bounds();
  home_card_widget_->SetBounds(gfx::Tween::RectValueBetween(
      progress,
      GetBoundsForState(screen_bounds, from_state),
      GetBoundsForState(screen_bounds, to_state)));

  home_card_view_->SetStateProgress(from_state, to_state, progress);

  // TODO(mukai): signals the update to the window manager so that it shows the
  // intermediate visual state of overview mode.
}

void HomeCardImpl::OnOverviewModeEnter() {
  if (state_ == HIDDEN || state_ == VISIBLE_MINIMIZED)
    SetState(VISIBLE_BOTTOM);
}

void HomeCardImpl::OnOverviewModeExit() {
  SetState(VISIBLE_MINIMIZED);
}

void HomeCardImpl::OnSplitViewModeEnter() {
}

void HomeCardImpl::OnSplitViewModeExit() {
}

void HomeCardImpl::OnWindowActivated(aura::Window* gained_active,
                                     aura::Window* lost_active) {
  if (state_ != HIDDEN &&
      gained_active != home_card_widget_->GetNativeWindow()) {
    SetState(VISIBLE_MINIMIZED);
  }
}

// static
HomeCard* HomeCard::Create(AppModelBuilder* model_builder) {
  (new HomeCardImpl(model_builder))->Init();
  DCHECK(instance);
  return instance;
}

// static
void HomeCard::Shutdown() {
  DCHECK(instance);
  delete instance;
  instance = NULL;
}

// static
HomeCard* HomeCard::Get() {
  DCHECK(instance);
  return instance;
}

}  // namespace athena
