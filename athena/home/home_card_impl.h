// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_HOME_CARD_IMPL_H_
#define ATHENA_HOME_HOME_CARD_IMPL_H_

#include "athena/athena_export.h"
#include "athena/home/home_card_gesture_manager.h"
#include "athena/home/public/home_card.h"
#include "athena/home/public/search_controller_factory.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/wm/public/window_manager_observer.h"

namespace app_list {
class AppListViewDelegate;
}

namespace aura {
class Window;
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
class HomeCardLayoutManager;
class HomeCardView;

class ATHENA_EXPORT HomeCardImpl : public HomeCard,
                                   public AcceleratorHandler,
                                   public HomeCardGestureManager::Delegate,
                                   public WindowManagerObserver {
 public:
  HomeCardImpl(scoped_ptr<AppModelBuilder> model_builder,
               scoped_ptr<SearchControllerFactory> search_factory);
  ~HomeCardImpl() override;

  void Init();

  aura::Window* GetHomeCardWindowForTest() const;

 private:
  enum Command {
    COMMAND_SHOW_HOME_CARD,
  };
  void InstallAccelerators();

  void ResetQuery();

  // Overridden from HomeCard:
  void SetState(HomeCard::State state) override;
  State GetState() override;
  void UpdateVirtualKeyboardBounds(const gfx::Rect& bounds) override;

  // AcceleratorHandler:
  bool IsCommandEnabled(int command_id) const override;
  bool OnAcceleratorFired(int command_id,
                          const ui::Accelerator& accelerator) override;

  // HomeCardGestureManager::Delegate:
  void OnGestureEnded(HomeCard::State final_state, bool is_fling) override;
  void OnGestureProgressed(HomeCard::State from_state,
                           HomeCard::State to_state,
                           float progress) override;

  // WindowManagerObserver:
  void OnOverviewModeEnter() override;
  void OnOverviewModeExit() override;
  void OnSplitViewModeEnter() override;
  void OnSplitViewModeExit() override;

  scoped_ptr<AppModelBuilder> model_builder_;
  scoped_ptr<SearchControllerFactory> search_factory_;

  HomeCard::State state_;

  // original_state_ is the state which the home card should go back to after
  // the virtual keyboard is hidden.
  HomeCard::State original_state_;

  views::Widget* home_card_widget_;
  HomeCardView* home_card_view_;
  scoped_ptr<app_list::AppListViewDelegate> view_delegate_;
  HomeCardLayoutManager* layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardImpl);
};

}  // namespace athena

#endif  // ATHENA_HOME_HOME_CARD_IMPL_H_
