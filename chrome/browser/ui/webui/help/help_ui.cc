// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/help/help_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"

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

  int product_logo = 0;
  switch (chrome::GetChannel()) {
#if defined(GOOGLE_CHROME_BUILD)
    case version_info::Channel::CANARY:
      product_logo = IDR_PRODUCT_LOGO_32_CANARY;
      break;
    case version_info::Channel::DEV:
      product_logo = IDR_PRODUCT_LOGO_32_DEV;
      break;
    case version_info::Channel::BETA:
      product_logo = IDR_PRODUCT_LOGO_32_BETA;
      break;
    case version_info::Channel::STABLE:
      product_logo = IDR_PRODUCT_LOGO_32;
      break;
#else
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
    case version_info::Channel::STABLE:
      NOTREACHED();
#endif
    case version_info::Channel::UNKNOWN:
      product_logo = IDR_PRODUCT_LOGO_32;
      break;
  }

  source->AddResourcePath("current-channel-logo", product_logo);
  return source;
}

}  // namespace

HelpUI::HelpUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = CreateAboutPageHTMLSource();

  HelpHandler* handler = new HelpHandler();
  base::DictionaryValue localized_strings;
  HelpHandler::GetLocalizedValues(&localized_strings);
  source->AddLocalizedStrings(localized_strings);
  content::WebUIDataSource::Add(profile, source);
  web_ui->AddMessageHandler(handler);
}

HelpUI::~HelpUI() {
}
