// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/help/help_handler.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"

namespace {

content::WebUIDataSource* CreateAboutPageHTMLSource() {
  content::WebUIDataSource* source =
      ChromeWebUIDataSource::Create(chrome::kChromeUIHelpFrameHost);

  source->SetJsonPath("strings.js");
  source->SetUseJsonJSFormatV2();
  source->AddResourcePath("help.js", IDR_HELP_JS);
  source->SetDefaultResource(IDR_HELP_HTML);
  source->DisableDenyXFrameOptions();
  return source;
}

}  // namespace

HelpUI::HelpUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = CreateAboutPageHTMLSource();

  HelpHandler* handler = new HelpHandler();
  handler->GetLocalizedValues(source);
  ChromeURLDataManager::AddWebUIDataSource(profile, source);
  web_ui->AddMessageHandler(handler);
}

HelpUI::~HelpUI() {
}
