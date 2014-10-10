// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/chrome/chrome_search_controller_factory.h"

#include "athena/extensions/chrome/app_list_controller_delegate_athena.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_controller_factory.h"

namespace athena {

ChromeSearchControllerFactory::ChromeSearchControllerFactory(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ChromeSearchControllerFactory::~ChromeSearchControllerFactory() {
}

scoped_ptr<app_list::SearchController> ChromeSearchControllerFactory::Create(
    app_list::SearchBoxModel* search_box,
    app_list::AppListModel::SearchResults* results) {
  list_controller_.reset(new AppListControllerDelegateAthena());
  return app_list::CreateSearchController(
      Profile::FromBrowserContext(browser_context_),
      search_box,
      results,
      list_controller_.get());
}

scoped_ptr<SearchControllerFactory> CreateSearchControllerFactory(
    content::BrowserContext* context) {
  return make_scoped_ptr(new ChromeSearchControllerFactory(context));
}

}  // namespace athena
