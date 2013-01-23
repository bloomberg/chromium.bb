// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/extensions/command_handler.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "chrome/browser/ui/webui/extensions/install_extension_handler.h"
#include "chrome/browser/ui/webui/extensions/pack_extension_handler.h"
#include "chrome/browser/ui/webui/generic_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"

namespace {

content::WebUIDataSource* CreateExtensionsHTMLSource() {
  content::WebUIDataSource* source =
      ChromeWebUIDataSource::Create(chrome::kChromeUIExtensionsFrameHost);

  source->SetUseJsonJSFormatV2();
  source->SetJsonPath("strings.js");
  source->AddResourcePath("extensions.js", IDR_EXTENSIONS_JS);
  source->AddResourcePath("extension_command_list.js",
                          IDR_EXTENSION_COMMAND_LIST_JS);
  source->AddResourcePath("extension_list.js", IDR_EXTENSION_LIST_JS);
  source->SetDefaultResource(IDR_EXTENSIONS_HTML);
  source->DisableDenyXFrameOptions();
  return source;
}

}  // namespace

ExtensionsUI::ExtensionsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = CreateExtensionsHTMLSource();

  ExtensionSettingsHandler* handler = new ExtensionSettingsHandler();
  handler->GetLocalizedValues(source);
  web_ui->AddMessageHandler(handler);

  PackExtensionHandler* pack_handler = new PackExtensionHandler();
  pack_handler->GetLocalizedValues(source);
  web_ui->AddMessageHandler(pack_handler);

  extensions::CommandHandler* commands_handler =
      new extensions::CommandHandler(profile);
  commands_handler->GetLocalizedValues(source);
  web_ui->AddMessageHandler(commands_handler);

  InstallExtensionHandler* install_extension_handler =
      new InstallExtensionHandler();
  install_extension_handler->GetLocalizedValues(source);
  web_ui->AddMessageHandler(install_extension_handler);

  web_ui->AddMessageHandler(new GenericHandler());
  ChromeURLDataManager::AddWebUIDataSource(profile, source);
}

ExtensionsUI::~ExtensionsUI() {
}
