// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_PUBLIC_SEARCH_CONTROLLER_FACTORY_H_
#define ATHENA_HOME_PUBLIC_SEARCH_CONTROLLER_FACTORY_H_

#include "athena/athena_export.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search_controller.h"

namespace app_list {
class SearchBoxModel;
}

namespace athena {

class ATHENA_EXPORT SearchControllerFactory {
 public:
  virtual ~SearchControllerFactory() {}

  virtual scoped_ptr<app_list::SearchController> Create(
      app_list::SearchBoxModel* search_box,
      app_list::AppListModel::SearchResults* results) = 0;
};

}  // namespace athena

#endif  // ATHENA_HOME_PUBLIC_SEARCH_CONTROLLER_FACTORY_H_
