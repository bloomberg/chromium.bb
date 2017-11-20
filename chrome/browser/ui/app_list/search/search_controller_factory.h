// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_FACTORY_H_

#include <memory>

#include "ash/app_list/model/app_list_model.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

class SearchController;

// Build a SearchController instance with the profile.
std::unique_ptr<SearchController> CreateSearchController(
    Profile* profile,
    AppListModel* model,
    AppListControllerDelegate* list_controller);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_FACTORY_H_
