// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/public/home_card.h"

#include <cmath>
#include <limits>

#include "athena/common/container_priorities.h"
#include "athena/home/app_list_view_delegate.h"
#include "athena/home/bottom_home_view.h"
#include "athena/home/minimized_home.h"
#include "athena/home/public/app_model_builder.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/wm/public/window_manager.h"
#include "athena/wm/public/window_manager_observer.h"
#include "base/bind.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/contents_view.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace athena {
namespace {

HomeCard* instance = NULL;

gfx::Rect GetBoundsForState(const gfx::Rect& screen_bounds,
                            HomeCard::State state) {
  const int kHomeCardHeight = 150;
  const int kHomeCardMinimizedHeight = 8;

  switch (state) {
    case HomeCard::HIDDEN:
      break;

    case HomeCard::VISIBLE_CENTERED:
      return screen_bounds;

    case HomeCard::VISIBLE_BOTTOM:
      return gfx::Rect(0,
                       screen_bounds.bottom() - kHomeCardHeight,
                       screen_bounds.width(),
                       kHomeCardHeight);
    case HomeCard::VISIBLE_MINIMIZED:
      return gfx::Rect(0,
                       screen_bounds.bottom() - kHomeCardMinimizedHeight,
                       screen_bounds.width(),
                       kHomeCardMinimizedHeight);
  }

  NOTREACHED();
  return gfx::Rect();
}

// Makes sure the homecard is center-aligned horizontally and bottom-aligned
// vertically.
class HomeCardLayoutManager : public aura::LayoutManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual HomeCard::State GetState() = 0;
    virtual aura::Window* GetNativeWindow() = 0;
  };

  explicit HomeCardLayoutManager(Delegate* delegate)
      : delegate_(delegate) {}

  virtual ~HomeCardLayoutManager() {}

  void Layout() {
    aura::Window* home_card = delegate_->GetNativeWindow();
    // |home_card| could be detached from the root window (e.g. when it is being
    // destroyed).
    if (!home_card || !home_card->GetRootWindow())
      return;

    SetChildBoundsDirect(home_card, GetBoundsForState(
        home_card->GetRootWindow()->bounds(), delegate_->GetState()));
  }

 private:
  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE { Layout(); }
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE { Layout(); }
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {
    Layout();
  }
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {
    Layout();
  }
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, requested_bounds);
  }

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardLayoutManager);
};

class HomeCardGestureManager {
 public:
  class Delegate {
   public:
    // Called when the gesture has ended. The state of the home card will
    // end up with |final_state|.
    virtual void OnGestureEnded(HomeCard::State final_state) = 0;

    // Called when the gesture position is updated so that |delegate| should
    // update the visual. The arguments represent the state of the current
    // gesture position is switching from |from_state| to |to_state|, and
    // the level of the progress is at |progress|, which is 0 to 1.
    // |from_state| and |to_state| could be same. For example, if the user moves
    // the finger down to the bottom of the screen, both states are MINIMIZED.
    // In that case |progress| is 0.
    virtual void OnGestureProgressed(
        HomeCard::State from_state,
        HomeCard::State to_state,
        float progress) = 0;
  };

  HomeCardGestureManager(Delegate* delegate,
                         const gfx::Rect& screen_bounds)
      : delegate_(delegate),
        last_state_(HomeCard::Get()->GetState()),
        y_offset_(0),
        last_estimated_top_(0),
        screen_bounds_(screen_bounds) {}

  void ProcessGestureEvent(ui::GestureEvent* event) {
    switch (event->type()) {
      case ui::ET_GESTURE_SCROLL_BEGIN:
        y_offset_ = event->location().y();
        event->SetHandled();
        break;
      case ui::ET_GESTURE_SCROLL_END:
        event->SetHandled();
        delegate_->OnGestureEnded(GetClosestState());
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        UpdateScrollState(*event);
        break;
      case ui::ET_SCROLL_FLING_START: {
        const ui::GestureEventDetails& details = event->details();
        const float kFlingCompletionVelocity = 100.0f;
        if (::fabs(details.velocity_y()) > kFlingCompletionVelocity) {
          int step = (details.velocity_y() > 0) ? 1 : -1;
          int new_state = static_cast<int>(last_state_) + step;
          if (new_state >= HomeCard::VISIBLE_CENTERED &&
              new_state <= HomeCard::VISIBLE_MINIMIZED) {
            last_state_ = static_cast<HomeCard::State>(new_state);
          }
          delegate_->OnGestureEnded(last_state_);
        }
        break;
      }
      default:
        // do nothing.
        break;
    }
  }

 private:
  HomeCard::State GetClosestState() {
    // The top position of the bounds for one smaller state than the current
    // one.
    int smaller_top = -1;
    for (int i = HomeCard::VISIBLE_MINIMIZED;
         i >= HomeCard::VISIBLE_CENTERED; --i) {
      HomeCard::State state = static_cast<HomeCard::State>(i);
      int top = GetBoundsForState(screen_bounds_, state).y();
      if (last_estimated_top_ == top) {
        return state;
      } else if (last_estimated_top_ > top) {
        if (smaller_top < 0)
          return state;

        if (smaller_top - last_estimated_top_ > (smaller_top - top) / 5) {
          return state;
        } else {
          return static_cast<HomeCard::State>(i + 1);
        }
      }
      smaller_top = top;
    }

    NOTREACHED();
    return last_state_;
  }

  void UpdateScrollState(const ui::GestureEvent& event) {
    last_estimated_top_ = event.root_location().y() - y_offset_;

    // The bounds which is at one smaller state than the current one.
    gfx::Rect smaller_bounds;

    for (int i = HomeCard::VISIBLE_MINIMIZED;
         i >= HomeCard::VISIBLE_CENTERED; --i) {
      HomeCard::State state = static_cast<HomeCard::State>(i);
      const gfx::Rect bounds = GetBoundsForState(screen_bounds_, state);
      if (last_estimated_top_ == bounds.y()) {
        delegate_->OnGestureProgressed(last_state_, state, 1.0f);
        last_state_ = state;
        return;
      } else if (last_estimated_top_ > bounds.y()) {
        if (smaller_bounds.IsEmpty()) {
          // Smaller than minimized -- returning the minimized bounds.
          delegate_->OnGestureProgressed(last_state_, state, 1.0f);
        } else {
          // The finger is between two states.
          float progress =
              static_cast<float>((smaller_bounds.y() - last_estimated_top_)) /
              (smaller_bounds.y() - bounds.y());
          if (last_state_ == state) {
            if (event.details().scroll_y() > 0) {
              state = static_cast<HomeCard::State>(state + 1);
              progress = 1.0f - progress;
            } else {
              last_state_ = static_cast<HomeCard::State>(last_state_ + 1);
            }
          }
          delegate_->OnGestureProgressed(last_state_, state, progress);
        }
        last_state_ = state;
        return;
      }
      smaller_bounds = bounds;
    }
    NOTREACHED();
  }

  Delegate* delegate_;
  HomeCard::State last_state_;

  // The offset from the top edge of the home card and the initial position of
  // gesture.
  int y_offset_;

  // The estimated top edge of the home card after the last touch event.
  int last_estimated_top_;

  // The bounds of the screen to compute the home card bounds.
  gfx::Rect screen_bounds_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardGestureManager);
};

// The container view of home card contents of each state.
class HomeCardView : public views::WidgetDelegateView {
 public:
  HomeCardView(app_list::AppListViewDelegate* view_delegate,
               aura::Window* container,
               HomeCardGestureManager::Delegate* gesture_delegate)
      : gesture_delegate_(gesture_delegate) {
    bottom_view_ = new BottomHomeView(view_delegate);
    AddChildView(bottom_view_);

    main_view_ = new app_list::AppListMainView(
        view_delegate, 0 /* initial_apps_page */, container);
    AddChildView(main_view_);
    main_view_->set_background(
        views::Background::CreateSolidBackground(SK_ColorWHITE));

    minimized_view_ = CreateMinimizedHome();
    AddChildView(minimized_view_);
  }

  void SetState(HomeCard::State state) {
    bottom_view_->SetVisible(state == HomeCard::VISIBLE_BOTTOM);
    main_view_->SetVisible(state == HomeCard::VISIBLE_CENTERED);
    minimized_view_->SetVisible(state == HomeCard::VISIBLE_MINIMIZED);
    if (state == HomeCard::VISIBLE_CENTERED) {
      app_list::ContentsView* contents_view = main_view_->contents_view();
      contents_view->SetActivePage(contents_view->GetPageIndexForNamedPage(
          app_list::ContentsView::NAMED_PAGE_START));
    }

    if (state == HomeCard::VISIBLE_MINIMIZED)
      shadow_.reset();
    // Do not create the shadow yet. Instead, create it in OnWidgetMove(), to
    // make sure that widget has been resized correctly (because the size of the
    // shadow depends on the size of the widget).
  }

  void ClearGesture() {
    gesture_manager_.reset();
  }

  // views::View:
  virtual void Layout() OVERRIDE {
    for (int i = 0; i < child_count(); ++i) {
      views::View* child = child_at(i);
      if (child->visible()) {
        child->SetBoundsRect(bounds());
        return;
      }
    }

    // One of the child views has to be visible.
    NOTREACHED();
  }
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

 private:
  // views::WidgetDelegate:
  virtual void OnWidgetMove() OVERRIDE {
    if (!minimized_view_->visible()) {
      aura::Window* window = GetWidget()->GetNativeWindow();
      if (!shadow_) {
        shadow_.reset(new wm::Shadow());
        shadow_->Init(wm::Shadow::STYLE_ACTIVE);
        shadow_->SetContentBounds(gfx::Rect(window->bounds().size()));
        shadow_->layer()->SetVisible(true);

        ui::Layer* layer = window->layer();
        layer->Add(shadow_->layer());
      } else {
        shadow_->SetContentBounds(gfx::Rect(window->bounds().size()));
      }
    }
  }

  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  app_list::AppListMainView* main_view_;
  BottomHomeView* bottom_view_;
  views::View* minimized_view_;
  scoped_ptr<wm::Shadow> shadow_;
  scoped_ptr<HomeCardGestureManager> gesture_manager_;
  HomeCardGestureManager::Delegate* gesture_delegate_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardView);
};

class HomeCardImpl : public HomeCard,
                     public AcceleratorHandler,
                     public HomeCardLayoutManager::Delegate,
                     public HomeCardGestureManager::Delegate,
                     public WindowManagerObserver,
                     public aura::client::ActivationChangeObserver {
 public:
  explicit HomeCardImpl(AppModelBuilder* model_builder);
  virtual ~HomeCardImpl();

  void Init();

 private:
  enum Command {
    COMMAND_SHOW_HOME_CARD,
  };
  void InstallAccelerators();

  // Overridden from HomeCard:
  virtual void SetState(State state) OVERRIDE;
  virtual State GetState() OVERRIDE;
  virtual void RegisterSearchProvider(
      app_list::SearchProvider* search_provider) OVERRIDE;
  virtual void UpdateVirtualKeyboardBounds(
      const gfx::Rect& bounds) OVERRIDE;

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE { return true; }
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) OVERRIDE {
    DCHECK_EQ(COMMAND_SHOW_HOME_CARD, command_id);

    if (state_ == VISIBLE_CENTERED && original_state_ != VISIBLE_BOTTOM)
      SetState(VISIBLE_MINIMIZED);
    else if (state_ == VISIBLE_MINIMIZED)
      SetState(VISIBLE_CENTERED);
    return true;
  }

  // HomeCardLayoutManager::Delegate:
  virtual aura::Window* GetNativeWindow() OVERRIDE {
    if (state_ == HIDDEN)
      return NULL;

    return home_card_widget_ ? home_card_widget_->GetNativeWindow() : NULL;
  }

  // HomeCardGestureManager::Delegate:
  virtual void OnGestureEnded(State final_state) OVERRIDE {
    home_card_view_->ClearGesture();
    if (state_ != final_state &&
        (state_ == VISIBLE_MINIMIZED || final_state == VISIBLE_MINIMIZED)) {
      WindowManager::GetInstance()->ToggleOverview();
    } else {
      state_ = final_state;
      home_card_view_->SetState(final_state);
      layout_manager_->Layout();
    }
  }

  virtual void OnGestureProgressed(
      State from_state, State to_state, float progress) OVERRIDE {
    // Do not update |state_| but update the look of home_card_view.
    // TODO(mukai): allow mixed visual of |from_state| and |to_state|.
    home_card_view_->SetState(to_state);

    gfx::Rect screen_bounds =
        home_card_widget_->GetNativeWindow()->GetRootWindow()->bounds();
    home_card_widget_->SetBounds(gfx::Tween::RectValueBetween(
        progress,
        GetBoundsForState(screen_bounds, from_state),
        GetBoundsForState(screen_bounds, to_state)));

    // TODO(mukai): signals the update to the window manager so that it shows
    // the intermediate visual state of overview mode.
  }

  // WindowManagerObserver:
  virtual void OnOverviewModeEnter() OVERRIDE {
    SetState(VISIBLE_BOTTOM);
  }

  virtual void OnOverviewModeExit() OVERRIDE {
    SetState(VISIBLE_MINIMIZED);
  }

  // aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE {
    if (state_ != HIDDEN &&
        gained_active != home_card_widget_->GetNativeWindow()) {
      SetState(VISIBLE_MINIMIZED);
    }
  }

  scoped_ptr<AppModelBuilder> model_builder_;

  HomeCard::State state_;

  // original_state_ is the state which the home card should go back to after
  // the virtual keyboard is hidden.
  HomeCard::State original_state_;

  views::Widget* home_card_widget_;
  HomeCardView* home_card_view_;
  scoped_ptr<AppListViewDelegate> view_delegate_;
  HomeCardLayoutManager* layout_manager_;
  aura::client::ActivationClient* activation_client_;  // Not owned

  // Right now HomeCard allows only one search provider.
  // TODO(mukai): port app-list's SearchController and Mixer.
  scoped_ptr<app_list::SearchProvider> search_provider_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardImpl);
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
  WindowManager::GetInstance()->AddObserver(this);
}

HomeCardImpl::~HomeCardImpl() {
  DCHECK(instance);
  WindowManager::GetInstance()->RemoveObserver(this);
  if (activation_client_)
    activation_client_->RemoveObserver(this);
  home_card_widget_->CloseNow();
  instance = NULL;
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
    if (state_ == VISIBLE_CENTERED)
      home_card_widget_->Show();
    else
      home_card_widget_->ShowInactive();
    home_card_view_->SetState(state);
    layout_manager_->Layout();
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

void HomeCardImpl::Init() {
  InstallAccelerators();
  ScreenManager::ContainerParams params("HomeCardContainer", CP_HOME_CARD);
  params.can_activate_children = true;
  aura::Window* container = ScreenManager::Get()->CreateContainer(params);
  layout_manager_ = new HomeCardLayoutManager(this);

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

  SetState(VISIBLE_MINIMIZED);
  home_card_view_->Layout();

  activation_client_ =
      aura::client::GetActivationClient(container->GetRootWindow());
  if (activation_client_)
    activation_client_->AddObserver(this);
}

void HomeCardImpl::InstallAccelerators() {
  const AcceleratorData accelerator_data[] = {
      {TRIGGER_ON_PRESS, ui::VKEY_L, ui::EF_CONTROL_DOWN,
       COMMAND_SHOW_HOME_CARD, AF_NONE},
  };
  AcceleratorManager::Get()->RegisterAccelerators(
      accelerator_data, arraysize(accelerator_data), this);
}

}  // namespace

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
