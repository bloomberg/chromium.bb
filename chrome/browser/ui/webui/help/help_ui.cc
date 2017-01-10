// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_ui.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/help/help_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

content::WebUIDataSource* CreateAboutPageHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIHelpFrameHost);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("help.js", IDR_HELP_JS);
  source->AddResourcePath("help_page.js", IDR_HELP_PAGE_JS);
  source->AddResourcePath("channel_change_page.js", IDR_CHANNEL_CHANGE_PAGE_JS);
  source->SetDefaultResource(IDR_HELP_HTML);
  source->DisableDenyXFrameOptions();
  return source;
}

}  // namespace

HelpUI::HelpUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = CreateAboutPageHTMLSource();

  base::DictionaryValue localized_strings;
  HelpHandler::GetLocalizedValues(&localized_strings);
  source->AddLocalizedStrings(localized_strings);
  content::WebUIDataSource::Add(profile, source);
  web_ui->AddMessageHandler(base::MakeUnique<HelpHandler>());
}

HelpUI::~HelpUI() {
}
