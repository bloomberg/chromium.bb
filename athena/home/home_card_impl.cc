// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/public/home_card.h"

#include "athena/home/app_list_view_delegate.h"
#include "athena/home/minimized_home.h"
#include "athena/home/public/app_model_builder.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/wm/public/window_manager.h"
#include "athena/wm/public/window_manager_observer.h"
#include "base/bind.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

namespace athena {
namespace {

HomeCard* instance = NULL;

// Makes sure the homecard is center-aligned horizontally and bottom-aligned
// vertically.
class HomeCardLayoutManager : public aura::LayoutManager {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual int GetHomeCardHeight() const = 0;

    virtual int GetHorizontalMargin() const = 0;

    // TODO(mukai): Remove this when bubble is no longer used for
    // VISIBLE_CENTERED or VISIBLE_BOTTOM states.
    virtual bool HasShadow() const = 0;

    virtual aura::Window* GetNativeWindow() = 0;
  };

  explicit HomeCardLayoutManager(Delegate* delegate)
      : delegate_(delegate) {}
  virtual ~HomeCardLayoutManager() {}

  void UpdateVirtualKeyboardBounds(const gfx::Rect& bounds) {
    virtual_keyboard_bounds_ = bounds;
    Layout();
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
    SetChildBoundsDirect(child, gfx::Rect(requested_bounds.size()));
  }

  void Layout() {
    int height = delegate_->GetHomeCardHeight();
    int horiz_margin = delegate_->GetHorizontalMargin();
    aura::Window* home_card = delegate_->GetNativeWindow();
    // |home_card| could be detached from the root window (e.g. when it is being
    // destroyed).
    if (!home_card || !home_card->GetRootWindow())
      return;

    gfx::Rect screen_bounds = home_card->GetRootWindow()->bounds();
    if (!virtual_keyboard_bounds_.IsEmpty())
      screen_bounds.set_height(virtual_keyboard_bounds_.y());
    gfx::Rect card_bounds = screen_bounds;
    card_bounds.Inset(horiz_margin, screen_bounds.height() - height,
                      horiz_margin, 0);

    if (delegate_->HasShadow()) {
      // Currently the home card is provided as a bubble and the bounds has to
      // be increased to cancel the shadow.
      // TODO(mukai): stops using the bubble and remove this.
      const int kHomeCardShadowWidth = 30;
      card_bounds.Inset(-kHomeCardShadowWidth, -kHomeCardShadowWidth);
    }
    SetChildBoundsDirect(home_card, card_bounds);
  }

  Delegate* delegate_;
  gfx::Rect virtual_keyboard_bounds_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardLayoutManager);
};

class HomeCardImpl : public HomeCard,
                     public AcceleratorHandler,
                     public HomeCardLayoutManager::Delegate,
                     public MinimizedHomeDragDelegate,
                     public WindowManagerObserver {
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
  virtual void RegisterSearchProvider(
      app_list::SearchProvider* search_provider) OVERRIDE;
  virtual void UpdateVirtualKeyboardBounds(
      const gfx::Rect& bounds) OVERRIDE;

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE { return true; }
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) OVERRIDE {
    DCHECK_EQ(COMMAND_SHOW_HOME_CARD, command_id);
    if (state_ == HIDDEN)
      SetState(VISIBLE_CENTERED);
    else
      SetState(HIDDEN);
    return true;
  }

  // HomeCardLayoutManager::Delegate:
  virtual int GetHomeCardHeight() const OVERRIDE {
    const int kHomeCardHeight = 150;
    const int kHomeCardMinimizedHeight = 8;
    CHECK_NE(HIDDEN, state_);
    return state_ == VISIBLE_MINIMIZED ? kHomeCardMinimizedHeight :
                                         kHomeCardHeight;
  }

  virtual int GetHorizontalMargin() const OVERRIDE {
    CHECK_NE(HIDDEN, state_);
    const int kHomeCardHorizontalMargin = 50;
    return state_ == VISIBLE_MINIMIZED ? 0 : kHomeCardHorizontalMargin;
  }

  virtual bool HasShadow() const OVERRIDE {
    CHECK_NE(HIDDEN, state_);
    return state_ != VISIBLE_MINIMIZED;
  }

  virtual aura::Window* GetNativeWindow() OVERRIDE {
    switch (state_) {
      case HIDDEN:
        return NULL;
      case VISIBLE_MINIMIZED:
        return minimized_widget_ ? minimized_widget_->GetNativeWindow() : NULL;
      case VISIBLE_CENTERED:
      case VISIBLE_BOTTOM:
        return home_card_widget_ ? home_card_widget_->GetNativeWindow() : NULL;
    }
    return NULL;
  }

  // MinimizedHomeDragDelegate:
  virtual void OnDragUpCompleted() OVERRIDE {
    WindowManager::GetInstance()->ToggleOverview();
  }

  // WindowManagerObserver:
  virtual void OnOverviewModeEnter() OVERRIDE {
    SetState(VISIBLE_BOTTOM);
  }

  virtual void OnOverviewModeExit() OVERRIDE {
    SetState(VISIBLE_MINIMIZED);
  }

  scoped_ptr<AppModelBuilder> model_builder_;

  HomeCard::State state_;

  views::Widget* home_card_widget_;
  views::Widget* minimized_widget_;
  AppListViewDelegate* view_delegate_;
  HomeCardLayoutManager* layout_manager_;

  // Right now HomeCard allows only one search provider.
  // TODO(mukai): port app-list's SearchController and Mixer.
  scoped_ptr<app_list::SearchProvider> search_provider_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardImpl);
};

HomeCardImpl::HomeCardImpl(AppModelBuilder* model_builder)
    : model_builder_(model_builder),
      state_(VISIBLE_MINIMIZED),
      home_card_widget_(NULL),
      minimized_widget_(NULL),
      layout_manager_(NULL) {
  DCHECK(!instance);
  instance = this;
  WindowManager::GetInstance()->AddObserver(this);
}

HomeCardImpl::~HomeCardImpl() {
  DCHECK(instance);
  WindowManager::GetInstance()->RemoveObserver(this);
  home_card_widget_->CloseNow();
  minimized_widget_->CloseNow();
  view_delegate_ = NULL;
  instance = NULL;
}

void HomeCardImpl::SetState(HomeCard::State state) {
  // Update |state_| before changing the visibility of the widgets, so that
  // LayoutManager callbacks get the correct state.
  state_ = state;
  switch (state_) {
    case VISIBLE_MINIMIZED:
      home_card_widget_->Hide();
      minimized_widget_->Show();
      break;
    case HIDDEN:
      home_card_widget_->Hide();
      minimized_widget_->Hide();
      break;
    case VISIBLE_BOTTOM:
    case VISIBLE_CENTERED:
      home_card_widget_->Show();
      minimized_widget_->Hide();
      break;
  }
}

void HomeCardImpl::RegisterSearchProvider(
    app_list::SearchProvider* search_provider) {
  DCHECK(!search_provider_);
  search_provider_.reset(search_provider);
  view_delegate_->RegisterSearchProvider(search_provider_.get());
}

void HomeCardImpl::UpdateVirtualKeyboardBounds(
    const gfx::Rect& bounds) {
  if (state_ == VISIBLE_MINIMIZED) {
    if (bounds.IsEmpty())
      minimized_widget_->Show();
    else
      minimized_widget_->Hide();
  }
  layout_manager_->UpdateVirtualKeyboardBounds(bounds);
}

void HomeCardImpl::Init() {
  InstallAccelerators();
  ScreenManager::ContainerParams params("HomeCardContainer");
  params.can_activate_children = true;
  aura::Window* container = ScreenManager::Get()->CreateContainer(params);
  layout_manager_ = new HomeCardLayoutManager(this);

  container->SetLayoutManager(layout_manager_);
  wm::SetChildWindowVisibilityChangesAnimated(container);

  view_delegate_ = new AppListViewDelegate(model_builder_.get());
  if (search_provider_)
    view_delegate_->RegisterSearchProvider(search_provider_.get());
  app_list::AppListView* view = new app_list::AppListView(view_delegate_);
  view->InitAsBubbleAtFixedLocation(
      container,
      0 /* initial_apps_page */,
      gfx::Point(),
      views::BubbleBorder::FLOAT,
      true /* border_accepts_events */);
  home_card_widget_ = view->GetWidget();

  // Start off in the minimized state.
  minimized_widget_ = CreateMinimizedHome(container, this);
  SetState(VISIBLE_MINIMIZED);
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
