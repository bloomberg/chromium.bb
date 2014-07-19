// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_BOTTOM_HOME_VIEW_H_
#define ATHENA_HOME_BOTTOM_HOME_VIEW_H_

#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/views/view.h"

namespace app_list {
class AppListViewDelegate;
}

namespace athena {

// The view of 'VISIBLE_BOTTOM' state of home card, which occupies
// smaller area and provides limited functionalities.
class BottomHomeView : public views::View,
                       public app_list::SearchBoxViewDelegate {
 public:
  explicit BottomHomeView(app_list::AppListViewDelegate* delegate);
  virtual ~BottomHomeView();

 private:
  // Overridden from app_list::SearchBoxViewDelegate:
  virtual void QueryChanged(app_list::SearchBoxView* sender) OVERRIDE;

  app_list::AppListViewDelegate* view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BottomHomeView);
};

}  // namespace athena

#endif  // ATHENA_HOME_BOTTOM_HOME_VIEW_H_
