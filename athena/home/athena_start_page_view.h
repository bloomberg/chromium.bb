// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_ATHENA_START_PAGE_VIEW_H_
#define ATHENA_HOME_ATHENA_START_PAGE_VIEW_H_

#include "athena/home/public/home_card.h"
#include "ui/app_list/views/search_box_view_delegate.h"
#include "ui/views/view.h"

namespace app_list {
class AppListViewDelegate;
}

namespace athena {

// It will replace app_list::StartPageView in Athena UI in the future.
// Right now it's simply used for VISIBLE_BOTTOM state.
class AthenaStartPageView : public views::View,
                            public app_list::SearchBoxViewDelegate {
 public:
  explicit AthenaStartPageView(app_list::AppListViewDelegate* delegate);
  virtual ~AthenaStartPageView();

 private:
  // views::View:
  virtual void Layout() OVERRIDE;

  // app_list::SearchBoxViewDelegate:
  virtual void QueryChanged(app_list::SearchBoxView* sender) OVERRIDE;

  views::View* container_;

  DISALLOW_COPY_AND_ASSIGN(AthenaStartPageView);
};

}  // namespace athena

#endif  // ATHENA_HOME_ATHENA_START_PAGE_VIEW_H_
