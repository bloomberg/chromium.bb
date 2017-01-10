// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_list/start_page_ui.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sys_info.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/app_list/start_page_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

namespace app_list {

StartPageUI::StartPageUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<StartPageHandler>());
  InitDataSource();
}

StartPageUI::~StartPageUI() {}

void StartPageUI::InitDataSource() {
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIAppListStartPageHost));

  source->SetJsonPath("strings.js");

  source->AddResourcePath("start_page.css", IDR_APP_LIST_START_PAGE_CSS);
  source->AddResourcePath("start_page.js", IDR_APP_LIST_START_PAGE_JS);
  source->SetDefaultResource(IDR_APP_LIST_START_PAGE_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui()), source.release());
}

}  // namespace app_list
