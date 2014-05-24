// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/public/home_card.h"

#include "athena/home/home_card_delegate_view.h"
#include "athena/screen/public/screen_manager.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
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
    const int kHomeCardHeight = 50;
    const int kHomeCardHorizontalMargin = 50;
    if (container_->children().size() < 1)
      return;
    aura::Window* home_card = container_->children()[0];
    if (!home_card->IsVisible())
      return;
    gfx::Rect screen_bounds = home_card->GetRootWindow()->bounds();
    gfx::Rect card_bounds = screen_bounds;
    card_bounds.Inset(kHomeCardHorizontalMargin,
                      screen_bounds.height() - kHomeCardHeight,
                      kHomeCardHorizontalMargin,
                      0);
    SetChildBoundsDirect(home_card, card_bounds);
  }

  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardLayoutManager);
};

class HomeCardImpl : public HomeCard {
 public:
  HomeCardImpl();
  virtual ~HomeCardImpl();

  void Init();

 private:
  views::Widget* home_card_widget_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardImpl);
};

HomeCardImpl::HomeCardImpl() : home_card_widget_(NULL) {
  DCHECK(!instance);
  instance = this;
}

HomeCardImpl::~HomeCardImpl() {
  DCHECK(instance);
  home_card_widget_->CloseNow();
  instance = NULL;
}

void HomeCardImpl::Init() {
  aura::Window* container =
      ScreenManager::Get()->CreateContainer("HomeCardContainer");
  container->SetLayoutManager(new HomeCardLayoutManager(container));
  wm::SetChildWindowVisibilityChangesAnimated(container);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.delegate = new HomeCardDelegateView();
  params.parent = container;

  home_card_widget_ = new views::Widget;
  home_card_widget_->Init(params);
  home_card_widget_->GetNativeView()->SetName("HomeCardWidget");

  aura::Window* home_card_window = home_card_widget_->GetNativeView();
  wm::SetWindowVisibilityAnimationType(
      home_card_window, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  wm::SetWindowVisibilityAnimationTransition(home_card_window,
                                             wm::ANIMATE_BOTH);

  home_card_widget_->Show();
}

}  // namespace

// static
HomeCard* HomeCard::Create() {
  (new HomeCardImpl())->Init();
  DCHECK(instance);
  return instance;
}

// static
void HomeCard::Shutdown() {
  DCHECK(instance);
  delete instance;
  instance = NULL;
}

}  // namespace athena
