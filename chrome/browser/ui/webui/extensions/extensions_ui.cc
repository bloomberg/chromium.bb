// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/command_handler.h"
#include "chrome/browser/ui/webui/extensions/extension_error_handler.h"
#include "chrome/browser/ui/webui/extensions/extension_loader_handler.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "chrome/browser/ui/webui/extensions/install_extension_handler.h"
#include "chrome/browser/ui/webui/extensions/pack_extension_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/extensions/chromeos/kiosk_apps_handler.h"
#endif

namespace extensions {

namespace {

content::WebUIDataSource* CreateExtensionsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIExtensionsFrameHost);

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

  CommandHandler* commands_handler = new CommandHandler(profile);
  commands_handler->GetLocalizedValues(source);
  web_ui->AddMessageHandler(commands_handler);

  ExtensionErrorHandler* extension_error_handler =
      new ExtensionErrorHandler(profile);
  extension_error_handler->GetLocalizedValues(source);
  web_ui->AddMessageHandler(extension_error_handler);

  ExtensionLoaderHandler* extension_loader_handler =
      new ExtensionLoaderHandler(profile);
  extension_loader_handler->GetLocalizedValues(source);
  web_ui->AddMessageHandler(extension_loader_handler);

  InstallExtensionHandler* install_extension_handler =
      new InstallExtensionHandler();
  install_extension_handler->GetLocalizedValues(source);
  web_ui->AddMessageHandler(install_extension_handler);

#if defined(OS_CHROMEOS)
  chromeos::KioskAppsHandler* kiosk_app_handler =
      new chromeos::KioskAppsHandler();
  kiosk_app_handler->GetLocalizedValues(source);
  web_ui->AddMessageHandler(kiosk_app_handler);
#endif

  web_ui->AddMessageHandler(new MetricsHandler());

  content::WebUIDataSource::Add(profile, source);
}

ExtensionsUI::~ExtensionsUI() {}

// static
base::RefCountedMemory* ExtensionsUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.LoadDataResourceBytesForScale(IDR_EXTENSIONS_FAVICON, scale_factor);
}

}  // namespace extensions
