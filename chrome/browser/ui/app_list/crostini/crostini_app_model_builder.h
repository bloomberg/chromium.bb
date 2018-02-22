// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_APP_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_APP_MODEL_BUILDER_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_model_builder.h"

class AppListControllerDelegate;

// This class populates and maintains Crostini apps.
class CrostiniAppModelBuilder : public AppListModelBuilder {
 public:
  explicit CrostiniAppModelBuilder(AppListControllerDelegate* controller);
  ~CrostiniAppModelBuilder() override;

 private:
  // AppListModelBuilder:
  void BuildModel() override;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppModelBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CROSTINI_CROSTINI_APP_MODEL_BUILDER_H_
