// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_VIEW_FACTORY_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_VIEW_FACTORY_H_

#include "base/callback_forward.h"
#include "chrome/browser/ui/views/app_list/win/app_list_view_win.h"

class Profile;

namespace app_list {
class PaginationModel;
}

// Factory for AppListViews. Used to allow us to create fake views in tests.
class AppListViewFactory {
 public:
  virtual ~AppListViewFactory() {}
  virtual AppListViewWin* CreateAppListView(
      Profile* profile,
      app_list::PaginationModel* pagination_model,
      const base::Closure& on_should_dismiss) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_VIEW_FACTORY_H_
