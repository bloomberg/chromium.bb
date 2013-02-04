// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/app_launch_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/jstemplate_builder.h"

namespace chromeos {

namespace {

content::WebUIDataSource* CreateWebUIDataSource() {
  content::WebUIDataSource* source =
        content::WebUIDataSource::Create(chrome::kChromeUIAppLaunchHost);
  source->SetDefaultResource(IDR_APP_LAUNCH_SPLASH_HTML);
  source->SetUseJsonJSFormatV2();
  source->AddLocalizedString("appStartMessage", IDS_APP_START_MESSAGE);
  source->AddLocalizedString("productName", IDS_SHORT_PRODUCT_NAME);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("app_launch.js", IDR_APP_LAUNCH_SPLASH_JS);
  return source;
}

}  // namespace

AppLaunchUI::AppLaunchUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  // Localized strings.
  // Set up the chrome://theme/ source, for Chrome logo.
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
  // Add data source.
  content::WebUIDataSource::Add(profile, CreateWebUIDataSource());
}

}  // namespace chromeos
