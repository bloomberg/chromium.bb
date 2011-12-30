// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/options/extension_settings_handler.h"
#include "chrome/browser/ui/webui/shared_resources_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"

namespace {

ChromeWebUIDataSource* CreateExtensionsHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIExtensionsFrameHost);

  source->set_json_path("strings.js");
  source->add_resource_path("extensions.js", IDR_EXTENSIONS_JS);
  source->add_resource_path("extension_list.js", IDR_EXTENSION_LIST_JS);
  source->set_default_resource(IDR_EXTENSIONS_HTML);
  return source;
}

}  // namespace

ExtensionsUI::ExtensionsUI(TabContents* contents) : ChromeWebUI(contents) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  ChromeWebUIDataSource* source = CreateExtensionsHTMLSource();
  profile->GetChromeURLDataManager()->AddDataSource(source);
  profile->GetChromeURLDataManager()->AddDataSource(
      new SharedResourcesDataSource());

  ExtensionSettingsHandler* handler = new ExtensionSettingsHandler();
  handler->GetLocalizedValues(source->localized_strings());
  AddMessageHandler(handler);
}

ExtensionsUI::~ExtensionsUI() {
}
