// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_SHELL_SHELL_SEARCH_CONTROLLER_FACTORY_H_
#define ATHENA_EXTENSIONS_SHELL_SHELL_SEARCH_CONTROLLER_FACTORY_H_

#include "athena/home/public/search_controller_factory.h"

namespace content {
class BrowserContext;
}

namespace athena {

class ShellSearchControllerFactory : public SearchControllerFactory {
 public:
  explicit ShellSearchControllerFactory(
      content::BrowserContext* browser_context);
  ~ShellSearchControllerFactory() override;

  scoped_ptr<app_list::SearchController> Create(
      app_list::SearchBoxModel* search_box,
      app_list::AppListModel::SearchResults* results) override;

 private:
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ShellSearchControllerFactory);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_SHELL_SHELL_SEARCH_CONTROLLER_FACTORY_H_
