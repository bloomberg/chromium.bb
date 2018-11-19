// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_management/app_management_page_handler.h"

#include <utility>
#include <vector>

AppManagementPageHandler::AppManagementPageHandler(
    app_management::mojom::PageHandlerRequest request,
    app_management::mojom::PagePtr page,
    content::WebUI* web_ui)
    : binding_(this, std::move(request)), page_(std::move(page)) {}

AppManagementPageHandler::~AppManagementPageHandler() {}

void AppManagementPageHandler::GetApps() {
  std::vector<std::string> ids = {"test_id"};
  page_->OnAppsAdded(ids);
}
