// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_ATHENA_START_PAGE_VIEW_H_
#define ATHENA_HOME_ATHENA_START_PAGE_VIEW_H_

#include "athena/athena_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/app_list/app_list_item_list_observer.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/view.h"

namespace app_list {
class AppListModelObserver;
class AppListViewDelegate;
class SearchBoxView;
class SearchResultListView;
}

namespace athena {

class ATHENA_EXPORT AthenaStartPageView
    : public views::View,
      public app_list::SearchBoxViewDelegate,
      public app_list::AppListItemListObserver {
 public:
  class Observer {
   public:
    virtual void OnLayoutStateChanged(float new_state) = 0;
  };

  explicit AthenaStartPageView(app_list::AppListViewDelegate* delegate);
  ~AthenaStartPageView() override;

  // Requests the focus on the search box in the start page view.
  void RequestFocusOnSearchBox();

  // Updates the layout state. See the comment of |layout_state_| field.
  void SetLayoutState(float layout_state);

  // Updates the layout state and move the subviews to the target location with
  // animation.
  void SetLayoutStateWithAnimation(float layout_state,
                                   gfx::Tween::Type tween_type);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class AthenaStartPageViewTest;

  static const char kViewClassName[];

  static size_t GetMaxIconNumForTest();

  // A struct which bundles the layout data of subviews.
  struct LayoutData {
    gfx::Rect search_box;
    gfx::Rect icons;
    gfx::Rect controls;
    float system_info_opacity;
    float logo_opacity;

    LayoutData();
  };

  // Returns the bounds for |VISIBLE_BOTTOM|.
  LayoutData CreateBottomBounds(int width);

  // Returns the bounds for |VISIBLE_CENTERED|.
  LayoutData CreateCenteredBounds(int width);

  void UpdateAppIcons();

  // Schedules the animation for the layout the search box and the search
  // results.
  void LayoutSearchResults(bool should_show_search_results);

  // Called when the animation of search box / search results layout has
  // completed.
  void OnSearchResultLayoutAnimationCompleted(bool should_show_search_results);

  // views::View:
  void Layout() override;
  bool OnKeyPressed(const ui::KeyEvent& key_event) override;

  // app_list::SearchBoxViewDelegate:
  void QueryChanged(app_list::SearchBoxView* sender) override;

  // app_list::AppListItemListObserver:
  void OnListItemAdded(size_t index, app_list::AppListItem* item) override;
  void OnListItemRemoved(size_t index, app_list::AppListItem* item) override;
  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       app_list::AppListItem* item) override;

  // Not owned.
  app_list::AppListViewDelegate* delegate_;

  // Views are owned through its hierarchy.
  views::View* system_info_view_;
  views::View* app_icon_container_;
  views::View* search_box_container_;
  views::View* control_icon_container_;
  views::View* logo_;
  app_list::SearchBoxView* search_box_view_;
  app_list::SearchResultListView* search_results_view_;

  // The expected height of |search_results_view_|
  int search_results_height_;

  // The state to specify how each of the subviews should be laid out, in the
  // range of [0, 1]. 0 means fully BOTTOM state, and 1 is fully CENTERED state.
  float layout_state_;

  ObserverList<Observer> observers_;

  base::WeakPtrFactory<AthenaStartPageView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AthenaStartPageView);
};

}  // namespace athena

#endif  // ATHENA_HOME_ATHENA_START_PAGE_VIEW_H_
