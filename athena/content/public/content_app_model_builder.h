// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_PUBLIC_CONTENT_APP_MODEL_BUILDER_H_
#define ATHENA_CONTENT_PUBLIC_CONTENT_APP_MODEL_BUILDER_H_

#include "athena/home/public/app_model_builder.h"
#include "base/macros.h"

namespace content {
class BrowserContext;
}

namespace athena {

class ATHENA_EXPORT ContentAppModelBuilder : public AppModelBuilder {
 public:
  explicit ContentAppModelBuilder(content::BrowserContext* browser_context);
  virtual ~ContentAppModelBuilder();

  virtual void PopulateApps(app_list::AppListModel* model) OVERRIDE;

 private:
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ContentAppModelBuilder);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_PUBLIC_CONTENT_APP_MODEL_BUILDER_H_
