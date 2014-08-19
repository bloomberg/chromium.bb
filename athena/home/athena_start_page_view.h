// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_ATHENA_START_PAGE_VIEW_H_
#define ATHENA_HOME_ATHENA_START_PAGE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/views/view.h"

namespace app_list {
class AppListViewDelegate;
class SearchBoxView;
class SearchResultListView;
}

namespace athena {

// It will replace app_list::StartPageView in Athena UI in the future.
// Right now it's simply used for VISIBLE_BOTTOM state.
class AthenaStartPageView : public views::View,
                            public app_list::SearchBoxViewDelegate {
 public:
  explicit AthenaStartPageView(app_list::AppListViewDelegate* delegate);
  virtual ~AthenaStartPageView();

  // Requests the focus on the search box in the start page view.
  void RequestFocusOnSearchBox();

 private:
  // Schedules the animation for the layout the search box and the search
  // results.
  void LayoutSearchResults(bool should_show_search_results);

  // Called when the animation of search box / search results layout has
  // completed.
  void OnSearchResultLayoutAnimationCompleted(bool should_show_search_results);

  // views::View:
  virtual void Layout() OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& key_event) OVERRIDE;

  // app_list::SearchBoxViewDelegate:
  virtual void QueryChanged(app_list::SearchBoxView* sender) OVERRIDE;

  // Not owned.
  app_list::AppListViewDelegate* delegate_;

  // Views are owned through its hierarchy.
  views::View* app_icon_container_;
  views::View* search_box_container_;
  views::View* control_icon_container_;
  views::View* logo_;
  app_list::SearchBoxView* search_box_view_;
  app_list::SearchResultListView* search_results_view_;

  // The expected height of |search_results_view_|
  int search_results_height_;

  base::WeakPtrFactory<AthenaStartPageView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AthenaStartPageView);
};

}  // namespace athena

#endif  // ATHENA_HOME_ATHENA_START_PAGE_VIEW_H_
