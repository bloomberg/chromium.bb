// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_HOME_CARD_IMPL_H_
#define ATHENA_HOME_HOME_CARD_IMPL_H_

#include "athena/athena_export.h"
#include "athena/home/home_card_gesture_manager.h"
#include "athena/home/public/home_card.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/wm/public/window_manager_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace app_list {
class SearchProvider;
}

namespace aura {
class Window;

namespace client {
class ActivationClient;
}
}

namespace gfx {
class Rect;
}

namespace ui {
class LayerOwner;
}

namespace views {
class Widget;
}

namespace athena {
class AppModelBuilder;
class AppListViewDelegate;
class HomeCardLayoutManager;
class HomeCardView;

class ATHENA_EXPORT HomeCardImpl
    : public HomeCard,
      public AcceleratorHandler,
      public HomeCardGestureManager::Delegate,
      public WindowManagerObserver,
      public aura::client::ActivationChangeObserver {
 public:
  explicit HomeCardImpl(AppModelBuilder* model_builder);
  virtual ~HomeCardImpl();

  void Init();

  aura::Window* GetHomeCardWindowForTest() const;

 private:
  enum Command {
    COMMAND_SHOW_HOME_CARD,
  };
  void InstallAccelerators();

  // Overridden from HomeCard:
  virtual void SetState(HomeCard::State state) OVERRIDE;
  virtual State GetState() OVERRIDE;
  virtual void RegisterSearchProvider(
      app_list::SearchProvider* search_provider) OVERRIDE;
  virtual void UpdateVirtualKeyboardBounds(
      const gfx::Rect& bounds) OVERRIDE;

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE;
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) OVERRIDE;

  // HomeCardGestureManager::Delegate:
  virtual void OnGestureEnded(HomeCard::State final_state,
                              bool is_fling) OVERRIDE;
  virtual void OnGestureProgressed(
      HomeCard::State from_state,
      HomeCard::State to_state,
      float progress) OVERRIDE;

  // WindowManagerObserver:
  virtual void OnOverviewModeEnter() OVERRIDE;
  virtual void OnOverviewModeExit() OVERRIDE;
  virtual void OnSplitViewModeEnter() OVERRIDE;
  virtual void OnSplitViewModeExit() OVERRIDE;

  // aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

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
  scoped_ptr<ui::LayerOwner> minimized_home_;

  // Right now HomeCard allows only one search provider.
  // TODO(mukai): port app-list's SearchController and Mixer.
  scoped_ptr<app_list::SearchProvider> search_provider_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardImpl);
};

}  // namespace athena

#endif  // ATHENA_HOME_HOME_CARD_IMPL_H_
