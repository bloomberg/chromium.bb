// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_CHROME_CHROME_SEARCH_CONTROLLER_FACTORY_H_
#define ATHENA_EXTENSIONS_CHROME_CHROME_SEARCH_CONTROLLER_FACTORY_H_

#include "athena/home/public/search_controller_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

namespace content {
class BrowserContext;
}

namespace athena {

class ChromeSearchControllerFactory : public SearchControllerFactory {
 public:
  explicit ChromeSearchControllerFactory(
      content::BrowserContext* browser_context);
  ~ChromeSearchControllerFactory() override;

  scoped_ptr<app_list::SearchController> Create(
      app_list::SearchBoxModel* search_box,
      app_list::AppListModel::SearchResults* results) override;

 private:
  content::BrowserContext* browser_context_;
  scoped_ptr<AppListControllerDelegate> list_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSearchControllerFactory);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_CHROME_CHROME_SEARCH_CONTROLLER_FACTORY_H_
