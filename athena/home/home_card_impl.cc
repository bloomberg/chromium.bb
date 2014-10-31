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
#include "athena/home/public/app_model_builder.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/util/container_priorities.h"
#include "athena/wm/public/window_manager.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/visibility_controller.h"

namespace athena {
namespace {

HomeCard* instance = nullptr;

const float kMinimizedHomeOpacity = 0.65f;

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

// The container view of home card contents of each state.
class HomeCardView : public views::WidgetDelegateView,
                     public AthenaStartPageView::Observer {
 public:
  HomeCardView(app_list::AppListViewDelegate* view_delegate,
               aura::Window* container,
               HomeCardGestureManager::Delegate* gesture_delegate)
      : background_(new views::View),
        main_view_(new AthenaStartPageView(view_delegate)),
        minimized_background_(new views::View()),
        drag_indicator_(new views::View()),
        gesture_delegate_(gesture_delegate) {
    background_->set_background(
        views::Background::CreateVerticalGradientBackground(SK_ColorLTGRAY,
                                                            SK_ColorWHITE));
    background_->SetPaintToLayer(true);
    background_->SetFillsBoundsOpaquely(false);
    AddChildView(background_);

    // Ideally AppListMainView should be used here and have AthenaStartPageView
    // as its child view, so that custom pages and apps grid are available in
    // the home card.
    // TODO(mukai): make it so after the detailed UI has been fixed.
    main_view_->AddObserver(this);
    AddChildView(main_view_);

    minimized_background_->set_background(
        views::Background::CreateSolidBackground(
            SkColorSetA(SK_ColorBLACK, 256 * kMinimizedHomeOpacity)));
    minimized_background_->SetPaintToLayer(true);
    minimized_background_->SetFillsBoundsOpaquely(false);
    minimized_background_->layer()->set_name("MinimizedBackground");
    AddChildView(minimized_background_);

    drag_indicator_->set_background(
        views::Background::CreateSolidBackground(SK_ColorWHITE));
    drag_indicator_->SetPaintToLayer(true);
    AddChildView(drag_indicator_);
  }

  ~HomeCardView() override { main_view_->RemoveObserver(this); }

  void SetStateProgress(HomeCard::State from_state,
                        HomeCard::State to_state,
                        float progress) {
    // TODO(mukai): not clear the focus, but simply close the virtual keyboard.
    GetFocusManager()->ClearFocus();
    if (from_state == HomeCard::VISIBLE_CENTERED)
      main_view_->SetLayoutState(1.0f - progress);
    else if (to_state == HomeCard::VISIBLE_CENTERED)
      main_view_->SetLayoutState(progress);

    float background_opacity = 1.0f;
    if (from_state == HomeCard::VISIBLE_MINIMIZED ||
        to_state == HomeCard::VISIBLE_MINIMIZED) {
      background_opacity = (from_state == HomeCard::VISIBLE_MINIMIZED)
                               ? progress
                               : (1.0f - progress);
    }
    background_->layer()->SetOpacity(background_opacity);
    minimized_background_->layer()->SetOpacity(1.0f - background_opacity);

    int background_height = kHomeCardHeight;
    if (from_state == HomeCard::VISIBLE_CENTERED ||
        to_state == HomeCard::VISIBLE_CENTERED) {
      gfx::Rect window_bounds = GetWidget()->GetWindowBoundsInScreen();
      background_height = window_bounds.height() - window_bounds.y();
    }
    gfx::Transform background_transform;
    background_transform.Scale(
        SK_MScalar1,
        SkIntToMScalar(background_height) / SkIntToMScalar(height()));
    background_->layer()->SetTransform(background_transform);

    gfx::Rect from_bounds = GetDragIndicatorBounds(from_state);
    gfx::Rect to_bounds = GetDragIndicatorBounds(to_state);
    if (from_bounds != to_bounds) {
      DCHECK_EQ(from_bounds.size().ToString(), to_bounds.size().ToString());
      drag_indicator_->SetBoundsRect(
          gfx::Tween::RectValueBetween(progress, from_bounds, to_bounds));
    }
  }

  void SetStateWithAnimation(HomeCard::State state,
                             gfx::Tween::Type tween_type) {
    float minimized_opacity =
        (state == HomeCard::VISIBLE_MINIMIZED) ? 1.0f : 0.0f;
    if (minimized_opacity !=
        minimized_background_->layer()->GetTargetOpacity()) {
      ui::ScopedLayerAnimationSettings settings(
          minimized_background_->layer()->GetAnimator());
      settings.SetTweenType(gfx::Tween::EASE_IN);
      minimized_background_->layer()->SetOpacity(minimized_opacity);
    }

    gfx::Transform background_transform;
    if (state != HomeCard::VISIBLE_CENTERED) {
      background_transform.Scale(
          SK_MScalar1,
          SkIntToMScalar(kHomeCardHeight) / SkIntToMScalar(height()));
    }
    float background_opacity = 1.0f - minimized_opacity;
    if (background_->layer()->GetTargetTransform() != background_transform ||
        background_->layer()->GetTargetOpacity() != background_opacity) {
      ui::ScopedLayerAnimationSettings settings(
          background_->layer()->GetAnimator());
      settings.SetTweenType(tween_type);
      background_->layer()->SetTransform(background_transform);
      background_->layer()->SetOpacity(background_opacity);
    }

    if (state == HomeCard::VISIBLE_CENTERED)
      main_view_->RequestFocusOnSearchBox();
    else
      GetWidget()->GetFocusManager()->ClearFocus();

    {
      ui::ScopedLayerAnimationSettings settings(
          drag_indicator_->layer()->GetAnimator());
      settings.SetTweenType(gfx::Tween::EASE_IN_OUT);
      drag_indicator_->SetBoundsRect(GetDragIndicatorBounds(state));
    }

    main_view_->SetLayoutStateWithAnimation(
        (state == HomeCard::VISIBLE_CENTERED) ? 1.0f : 0.0f, tween_type);
  }

  void ClearGesture() {
    gesture_manager_.reset();
  }

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (!gesture_manager_ &&
        event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
      gesture_manager_.reset(new HomeCardGestureManager(
          gesture_delegate_,
          GetWidget()->GetNativeWindow()->GetRootWindow()->bounds()));
    }

    if (gesture_manager_)
      gesture_manager_->ProcessGestureEvent(event);
  }
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (HomeCard::Get()->GetState() == HomeCard::VISIBLE_MINIMIZED &&
        event.IsLeftMouseButton() && event.GetClickCount() == 1) {
      athena::WindowManager::Get()->EnterOverview();
      return true;
    }
    return false;
  }

  gfx::Rect GetDragIndicatorBounds(HomeCard::State state) {
    gfx::Rect drag_indicator_bounds(
        GetContentsBounds().CenterPoint().x() - kHomeCardDragIndicatorWidth / 2,
        kHomeCardDragIndicatorMarginHeight,
        kHomeCardDragIndicatorWidth,
        kHomeCardDragIndicatorHeight);
    if (state == HomeCard::VISIBLE_CENTERED)
      drag_indicator_bounds.Offset(0, kSystemUIHeight);
    return drag_indicator_bounds;
  }

  void Layout() override {
    gfx::Rect contents_bounds = GetContentsBounds();
    background_->SetBoundsRect(contents_bounds);
    main_view_->SetBoundsRect(contents_bounds);
    minimized_background_->SetBoundsRect(contents_bounds);
    drag_indicator_->SetBoundsRect(
        GetDragIndicatorBounds(HomeCard::Get()->GetState()));
  }

 private:
  // views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }

  // AthenaStartPageView::Observer:
  void OnLayoutStateChanged(float new_state) override {
    if (new_state == 1.0f)
      HomeCard::Get()->SetState(HomeCard::VISIBLE_CENTERED);
  }

  views::View* background_;
  AthenaStartPageView* main_view_;
  views::View* minimized_background_;
  views::View* drag_indicator_;
  HomeCard::State state_;
  scoped_ptr<HomeCardGestureManager> gesture_manager_;
  HomeCardGestureManager::Delegate* gesture_delegate_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardView);
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

  home_card_view_ = new HomeCardView(view_delegate_.get(), container, this);
  home_card_widget_ = new views::Widget();
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  widget_params.parent = container;
  widget_params.delegate = home_card_view_;
  widget_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  home_card_widget_->Init(widget_params);

  SetState(VISIBLE_MINIMIZED);
  home_card_view_->Layout();

  AthenaEnv::Get()->SetDisplayWorkAreaInsets(
      gfx::Insets(0, 0, kHomeCardMinimizedHeight, 0));
}

aura::Window* HomeCardImpl::GetHomeCardWindowForTest() const {
  return home_card_widget_ ? home_card_widget_->GetNativeWindow() : nullptr;
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
    home_card_view_->SetStateWithAnimation(state, gfx::Tween::EASE_IN_OUT);
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
    home_card_view_->SetStateWithAnimation(state_, tween_type);
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
