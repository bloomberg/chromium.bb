// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/shell/shell_search_controller_factory.h"

#include "athena/extensions/shell/url_search_provider.h"
#include "ui/app_list/search_controller.h"

namespace athena {

ShellSearchControllerFactory::ShellSearchControllerFactory(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
}

ShellSearchControllerFactory::~ShellSearchControllerFactory() {
}

scoped_ptr<app_list::SearchController> ShellSearchControllerFactory::Create(
    app_list::SearchBoxModel* search_box,
    app_list::AppListModel::SearchResults* results) {
  scoped_ptr<app_list::SearchController> controller(
      new app_list::SearchController(
          search_box, results, nullptr /* no history */));
  controller->AddProvider(app_list::Mixer::MAIN_GROUP,
                          scoped_ptr<app_list::SearchProvider>(
                              new UrlSearchProvider(browser_context_)));
  return controller.Pass();
}

scoped_ptr<SearchControllerFactory> CreateSearchControllerFactory(
    content::BrowserContext* context) {
  return make_scoped_ptr(new ShellSearchControllerFactory(context));
}

}  // namespace athena
