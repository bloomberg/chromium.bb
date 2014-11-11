// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_HOME_CARD_VIEW_H_
#define ATHENA_HOME_HOME_CARD_VIEW_H_

#include "athena/athena_export.h"
#include "athena/home/home_card_gesture_manager.h"
#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/widget/widget_delegate.h"

namespace app_list {
class AppListMainView;
class AppListViewDelegate;
class SearchBoxView;
}

namespace aura {
class Window;
}

namespace views {
class View;
}

namespace athena {

// The container view of home card contents.
class ATHENA_EXPORT HomeCardView : public views::WidgetDelegateView {
 public:
  HomeCardView(app_list::AppListViewDelegate* view_delegate,
               HomeCardGestureManager::Delegate* gesture_delegate);
  ~HomeCardView() override;

  void Init();

  void SetStateProgress(HomeCard::State from_state,
                        HomeCard::State to_state,
                        float progress);

  void SetStateWithAnimation(HomeCard::State state,
                             gfx::Tween::Type tween_type,
                             const base::Closure& on_animation_ended);

  void ClearGesture();

  // views::View:
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void Layout() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HomeCardTest, AppListStates);

  gfx::Rect GetDragIndicatorBounds(HomeCard::State state);

  gfx::Rect GetMainViewBounds(HomeCard::State state);

  void UpdateMinimizedBackgroundVisibility();

  // views::WidgetDelegate:
  views::View* GetContentsView() override;

  views::View* background_;
  app_list::AppListMainView* main_view_;
  app_list::SearchBoxView* search_box_view_;
  views::View* minimized_background_;
  views::View* drag_indicator_;
  HomeCard::State state_;
  scoped_ptr<HomeCardGestureManager> gesture_manager_;
  HomeCardGestureManager::Delegate* gesture_delegate_;

  base::WeakPtrFactory<HomeCardView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HomeCardView);
};

}  // namespace athena

#endif  // ATHENA_HOME_CARD_VIEW_H_
