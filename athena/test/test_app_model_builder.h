// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_TEST_APP_MODEL_BUILDER_H_
#define ATHENA_TEST_TEST_APP_MODEL_BUILDER_H_

#include "athena/home/public/app_model_builder.h"
#include "base/macros.h"

namespace athena {

class TestAppModelBuilder : public AppModelBuilder {
 public:
  TestAppModelBuilder();
  virtual ~TestAppModelBuilder();

  // Overridden from AppModelBuilder:
  virtual void PopulateApps(app_list::AppListModel* model) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppModelBuilder);
};

}  // namespace athena

#endif  // ATHENA_TEST_TEST_APP_MODEL_BUILDER_H_
