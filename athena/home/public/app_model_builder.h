// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_PUBLIC_APP_MODEL_BUILDER_H_
#define ATHENA_HOME_PUBLIC_APP_MODEL_BUILDER_H_

#include "athena/athena_export.h"

namespace app_list {
class AppListModel;
}  // namespace app_list

namespace athena {

// An interface to fill the list of apps in the home card.
// TODO(mukai): integrate the interface with chrome/browser/ui/app_list/
// extension_app_model_builder.
class ATHENA_EXPORT AppModelBuilder {
 public:
  virtual ~AppModelBuilder() {}

  // Fills |model| with the currently available app_list::AppListItems.
  virtual void PopulateApps(app_list::AppListModel* model) = 0;
};

}  // namespace athena

#endif  // ATHENA_HOME_PUBLIC_APP_MODEL_BUILDER_H_
