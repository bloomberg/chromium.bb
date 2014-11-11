// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/home_card_impl.h"

#include <cmath>
#include <limits>

#include "athena/env/public/athena_env.h"
#include "athena/home/app_list_view_delegate.h"
#include "athena/home/home_card_constants.h"
#include "athena/home/home_card_view.h"
#include "athena/home/public/app_model_builder.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/util/container_priorities.h"
#include "athena/wm/public/window_manager.h"
#include "ui/app_list/search_box_model.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/visibility_controller.h"

namespace athena {
namespace {

HomeCard* instance = nullptr;

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
  HomeCardLayoutManager() : home_card_(nullptr) {}

  ~HomeCardLayoutManager() override {}

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

 private:
  // aura::LayoutManager:
  void OnWindowResized() override {
    Layout(false, gfx::Tween::LINEAR);
  }
  void OnWindowAddedToLayout(aura::Window* child) override {
    if (!home_card_) {
      home_card_ = child;
      Layout(false, gfx::Tween::LINEAR);
    }
  }
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {
    if (home_card_ == child)
      home_card_ = nullptr;
  }
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {
    if (home_card_ == child)
      Layout(false, gfx::Tween::LINEAR);
  }
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {
    SetChildBoundsDirect(child, requested_bounds);
  }

  aura::Window* home_card_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardLayoutManager);
};

HomeCardImpl::HomeCardImpl(scoped_ptr<AppModelBuilder> model_builder,
                           scoped_ptr<SearchControllerFactory> search_factory)
    : model_builder_(model_builder.Pass()),
      search_factory_(search_factory.Pass()),
      state_(HIDDEN),
      original_state_(VISIBLE_MINIMIZED),
      home_card_widget_(nullptr),
      home_card_view_(nullptr),
      layout_manager_(nullptr) {
  DCHECK(!instance);
  instance = this;
  WindowManager::Get()->AddObserver(this);
}

HomeCardImpl::~HomeCardImpl() {
  DCHECK(instance);
  WindowManager::Get()->RemoveObserver(this);
  home_card_widget_->CloseNow();

  // Reset the view delegate first as it access search provider during
  // shutdown.
  view_delegate_->GetModel()->RemoveObserver(this);
  view_delegate_.reset();
  instance = nullptr;
}

void HomeCardImpl::Init() {
  InstallAccelerators();
  ScreenManager::ContainerParams params("HomeCardContainer", CP_HOME_CARD);
  params.can_activate_children = true;
  aura::Window* container = ScreenManager::Get()->CreateContainer(params);
  layout_manager_ = new HomeCardLayoutManager();

  container->SetLayoutManager(layout_manager_);
  wm::SetChildWindowVisibilityChangesAnimated(container);

  view_delegate_.reset(
      new AppListViewDelegate(model_builder_.get(), search_factory_.get()));

  view_delegate_->GetModel()->AddObserver(this);
  home_card_view_ = new HomeCardView(view_delegate_.get(), this);
  home_card_widget_ = new views::Widget();
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.parent = container;
  widget_params.delegate = home_card_view_;
  widget_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  home_card_widget_->Init(widget_params);
  // AppListMainView in home card may move outside of home card layer partially
  // in an transition animation. This flag is set to clip the parts outside of
  // home card.
  home_card_widget_->GetNativeWindow()->layer()->SetMasksToBounds(true);

  home_card_view_->Init();
  SetState(VISIBLE_MINIMIZED);
  home_card_view_->Layout();

  AthenaEnv::Get()->SetDisplayWorkAreaInsets(
      gfx::Insets(0, 0, kHomeCardMinimizedHeight, 0));
}

aura::Window* HomeCardImpl::GetHomeCardWindowForTest() const {
  return home_card_widget_ ? home_card_widget_->GetNativeWindow() : nullptr;
}

void HomeCardImpl::ResetQuery() {
  view_delegate_->GetModel()->search_box()->SetText(base::string16());
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
  state_ = state;
  original_state_ = state;

  if (state_ == HIDDEN) {
    home_card_widget_->Hide();
  } else {
    if (state_ == VISIBLE_MINIMIZED)
      home_card_widget_->ShowInactive();
    else
      home_card_widget_->Show();

    // Query should be reset on state change to reset the main_view. Also it's
    // not possible to invoke ResetQuery() here, it causes a crash on search.
    home_card_view_->SetStateWithAnimation(
        state,
        gfx::Tween::EASE_IN_OUT,
        base::Bind(&HomeCardImpl::ResetQuery, base::Unretained(this)));
    layout_manager_->Layout(true, gfx::Tween::EASE_IN_OUT);
  }
}

HomeCard::State HomeCardImpl::GetState() {
  return state_;
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

  if (state_ == VISIBLE_CENTERED && original_state_ != VISIBLE_BOTTOM) {
    SetState(VISIBLE_MINIMIZED);
    WindowManager::Get()->ExitOverview();
  } else if (state_ == VISIBLE_MINIMIZED) {
    SetState(VISIBLE_CENTERED);
    WindowManager::Get()->EnterOverview();
  }
  return true;
}

void HomeCardImpl::OnGestureEnded(State final_state, bool is_fling) {
  home_card_view_->ClearGesture();
  if (state_ != final_state &&
      (state_ == VISIBLE_MINIMIZED || final_state == VISIBLE_MINIMIZED)) {
    SetState(final_state);
    if (WindowManager::Get()->IsOverviewModeActive())
      WindowManager::Get()->ExitOverview();
    else
      WindowManager::Get()->EnterOverview();
  } else {
    state_ = final_state;
    // When the animation happens after a fling, EASE_IN_OUT would cause weird
    // slow-down right after the finger release because of slow-in. Therefore
    // EASE_OUT is better.
    gfx::Tween::Type tween_type =
        is_fling ? gfx::Tween::EASE_OUT : gfx::Tween::EASE_IN_OUT;
    home_card_view_->SetStateWithAnimation(
        state_,
        tween_type,
        base::Bind(&HomeCardImpl::ResetQuery, base::Unretained(this)));
    layout_manager_->Layout(true, tween_type);
  }
}

void HomeCardImpl::OnGestureProgressed(
    State from_state, State to_state, float progress) {
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

void HomeCardImpl::OnAppListModelStateChanged(
    app_list::AppListModel::State old_state,
    app_list::AppListModel::State new_state) {
  // State change should not happen in minimized mode.
  DCHECK_NE(VISIBLE_MINIMIZED, state_);

  if (state_ == VISIBLE_BOTTOM) {
    if (old_state == app_list::AppListModel::STATE_START)
      SetState(VISIBLE_CENTERED);
    else
      DCHECK_EQ(app_list::AppListModel::STATE_START, new_state);
  }
}

// static
HomeCard* HomeCard::Create(scoped_ptr<AppModelBuilder> model_builder,
                           scoped_ptr<SearchControllerFactory> search_factory) {
  (new HomeCardImpl(model_builder.Pass(), search_factory.Pass()))->Init();
  DCHECK(instance);
  return instance;
}

// static
void HomeCard::Shutdown() {
  DCHECK(instance);
  delete instance;
  instance = nullptr;
}

// static
HomeCard* HomeCard::Get() {
  DCHECK(instance);
  return instance;
}

}  // namespace athena
