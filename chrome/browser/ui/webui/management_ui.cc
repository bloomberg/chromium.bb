// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"

namespace {

content::WebUIDataSource* CreateManagementUIHtmlSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIManagementHost);
  source->AddLocalizedString("title", IDS_MANAGEMENT_TITLE);
  source->SetJsonPath("strings.js");
  // Add required resources.
  source->AddResourcePath("management.css", IDR_MANAGEMENT_CSS);
  source->AddResourcePath("management.js", IDR_MANAGEMENT_JS);
  source->SetDefaultResource(IDR_MANAGEMENT_HTML);
  source->UseGzip();
  return source;
}

}  // namespace

ManagementUI::ManagementUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ManagementUIHandler>());
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui),
                                CreateManagementUIHtmlSource());
}

ManagementUI::~ManagementUI() {}
