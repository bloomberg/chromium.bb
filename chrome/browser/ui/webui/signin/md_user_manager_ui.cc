// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/md_user_manager_ui.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/signin_create_profile_handler.h"
#include "chrome/browser/ui/webui/signin/user_manager_screen_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/settings_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

MDUserManagerUI::MDUserManagerUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      signin_create_profile_handler_(new SigninCreateProfileHandler()),
      user_manager_screen_handler_(new UserManagerScreenHandler()) {
  // The web_ui object takes ownership of these handlers, and will
  // destroy them when it (the WebUI) is destroyed.
  web_ui->AddMessageHandler(signin_create_profile_handler_);
  web_ui->AddMessageHandler(user_manager_screen_handler_);

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  Profile* profile = Profile::FromWebUI(web_ui);
  // Set up the chrome://user-chooser/ source.
  content::WebUIDataSource::Add(profile, CreateUIDataSource(localized_strings));

#if defined(ENABLE_THEMES)
  // Set up the chrome://theme/ source
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
#endif
}

MDUserManagerUI::~MDUserManagerUI() {}

content::WebUIDataSource* MDUserManagerUI::CreateUIDataSource(
    const base::DictionaryValue& localized_strings) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMdUserManagerHost);
  source->AddLocalizedStrings(localized_strings);
  source->SetJsonPath("strings.js");

  // TODO(mahmadi): Add resource paths.

  return source;
}

void MDUserManagerUI::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  user_manager_screen_handler_->GetLocalizedValues(localized_strings);
  signin_create_profile_handler_->GetLocalizedValues(localized_strings);
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings);

#if defined(GOOGLE_CHROME_BUILD)
  localized_strings->SetString("buildType", "chrome");
#else
  localized_strings->SetString("buildType", "chromium");
#endif
}
