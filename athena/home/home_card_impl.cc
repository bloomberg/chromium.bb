// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/public/home_card.h"

#include "athena/home/app_list_view_delegate.h"
#include "athena/home/public/app_model_builder.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "ui/app_list/search_provider.h"
#include "ui/app_list/views/app_list_view.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

namespace athena {
namespace {

HomeCard* instance = NULL;

class HomeCardLayoutManager : public aura::LayoutManager {
 public:
  explicit HomeCardLayoutManager(aura::Window* container)
      : container_(container) {}
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
    const int kHomeCardHeight = 150;
    const int kHomeCardHorizontalMargin = 50;
    // Currently the home card is provided as a bubble and the bounds has to be
    // increased to cancel the shadow.
    // TODO(mukai): stops using the bubble and remove this.
    const int kHomeCardShadowWidth = 30;
    if (container_->children().size() < 1)
      return;
    aura::Window* home_card = container_->children()[0];
    if (!home_card->IsVisible())
      return;

    gfx::Rect screen_bounds = home_card->GetRootWindow()->bounds();
    if (!virtual_keyboard_bounds_.IsEmpty())
      screen_bounds.set_height(virtual_keyboard_bounds_.y());
    gfx::Rect card_bounds = screen_bounds;
    card_bounds.Inset(kHomeCardHorizontalMargin,
                      screen_bounds.height() - kHomeCardHeight,
                      kHomeCardHorizontalMargin,
                      0);
    card_bounds.Inset(-kHomeCardShadowWidth, -kHomeCardShadowWidth);
    SetChildBoundsDirect(home_card, card_bounds);
  }

  aura::Window* container_;
  gfx::Rect virtual_keyboard_bounds_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardLayoutManager);
};

class HomeCardImpl : public HomeCard, public AcceleratorHandler {
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

  scoped_ptr<AppModelBuilder> model_builder_;

  HomeCard::State state_;

  views::Widget* home_card_widget_;
  AppListViewDelegate* view_delegate_;
  HomeCardLayoutManager* layout_manager_;

  // Right now HomeCard allows only one search provider.
  // TODO(mukai): port app-list's SearchController and Mixer.
  scoped_ptr<app_list::SearchProvider> search_provider_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardImpl);
};

HomeCardImpl::HomeCardImpl(AppModelBuilder* model_builder)
    : model_builder_(model_builder),
      state_(HIDDEN),
      home_card_widget_(NULL),
      layout_manager_(NULL) {
  DCHECK(!instance);
  instance = this;
}

HomeCardImpl::~HomeCardImpl() {
  DCHECK(instance);
  home_card_widget_->CloseNow();
  view_delegate_ = NULL;
  instance = NULL;
}

void HomeCardImpl::SetState(HomeCard::State state) {
  if (state == HIDDEN)
    home_card_widget_->Hide();
  else
    home_card_widget_->Show();
  state_ = state;
}

void HomeCardImpl::RegisterSearchProvider(
    app_list::SearchProvider* search_provider) {
  DCHECK(!search_provider_);
  search_provider_.reset(search_provider);
  view_delegate_->RegisterSearchProvider(search_provider_.get());
}

void HomeCardImpl::UpdateVirtualKeyboardBounds(
    const gfx::Rect& bounds) {
  layout_manager_->UpdateVirtualKeyboardBounds(bounds);
}

void HomeCardImpl::Init() {
  InstallAccelerators();

  aura::Window* container =
      ScreenManager::Get()->CreateContainer("HomeCardContainer");
  layout_manager_ = new HomeCardLayoutManager(container);
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
  // TODO: the initial value might not be visible.
  state_ = VISIBLE_CENTERED;
  view->ShowWhenReady();
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
