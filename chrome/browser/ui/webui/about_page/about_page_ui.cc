// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/about_page/about_page_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/about_page/about_page_handler.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/shared_resources_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"

namespace {

ChromeWebUIDataSource* CreateAboutPageHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIAboutPageFrameHost);

  source->set_json_path("strings.js");
  source->add_resource_path("about_page.js", IDR_ABOUT_PAGE_JS);
  source->set_default_resource(IDR_ABOUT_PAGE_HTML);
  return source;
}

}  // namespace

AboutPageUI::AboutPageUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeWebUIDataSource* source = CreateAboutPageHTMLSource();
  profile->GetChromeURLDataManager()->AddDataSource(source);
  profile->GetChromeURLDataManager()->AddDataSource(
      new SharedResourcesDataSource());

  AboutPageHandler* handler = new AboutPageHandler();
  handler->GetLocalizedValues(source->localized_strings());
  web_ui->AddMessageHandler(handler);
}

AboutPageUI::~AboutPageUI() {
}
