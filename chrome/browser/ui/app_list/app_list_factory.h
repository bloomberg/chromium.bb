// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_FACTORY_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_FACTORY_H_

#include "base/callback_forward.h"
#include "chrome/browser/ui/app_list/app_list.h"

class Profile;

namespace app_list {
class PaginationModel;
}

// Factory for AppLists. Used to allow us to create fake app lists in tests.
class AppListFactory {
 public:
  virtual ~AppListFactory() {}
  virtual AppList* CreateAppList(Profile* profile,
                                 const base::Closure& on_should_dismiss) = 0;
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_FACTORY_H_
