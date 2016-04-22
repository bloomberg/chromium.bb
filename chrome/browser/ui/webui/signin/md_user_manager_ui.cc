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

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/ui/webui/signin/signin_supervised_user_import_handler.h"
#endif

MDUserManagerUI::MDUserManagerUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      signin_create_profile_handler_(new SigninCreateProfileHandler()),
      user_manager_screen_handler_(new UserManagerScreenHandler()) {
  // The web_ui object takes ownership of these handlers, and will
  // destroy them when it (the WebUI) is destroyed.
  web_ui->AddMessageHandler(signin_create_profile_handler_);
  web_ui->AddMessageHandler(user_manager_screen_handler_);
#if defined(ENABLE_SUPERVISED_USERS)
  signin_supervised_user_import_handler_ =
      new SigninSupervisedUserImportHandler();
  web_ui->AddMessageHandler(signin_supervised_user_import_handler_);
#endif

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

  source->AddResourcePath("control_bar.css", IDR_MD_CONTROL_BAR_CSS);
  source->AddResourcePath("control_bar.html", IDR_MD_CONTROL_BAR_HTML);
  source->AddResourcePath("control_bar.js", IDR_MD_CONTROL_BAR_JS);
  source->AddResourcePath("create_profile.css", IDR_MD_CREATE_PROFILE_CSS);
  source->AddResourcePath("create_profile.html", IDR_MD_CREATE_PROFILE_HTML);
  source->AddResourcePath("create_profile.js", IDR_MD_CREATE_PROFILE_JS);
  source->AddResourcePath("profile_browser_proxy.html",
                          IDR_MD_PROFILE_BROWSER_PROXY_HTML);
  source->AddResourcePath("profile_browser_proxy.js",
                          IDR_MD_PROFILE_BROWSER_PROXY_JS);
  source->AddResourcePath("strings.html", IDR_MD_USER_MANAGER_STRINGS_HTML);
  source->AddResourcePath("supervised_user_learn_more.css",
                          IDR_MD_SUPERVISED_USER_LEARN_MORE_CSS);
  source->AddResourcePath("supervised_user_learn_more.html",
                          IDR_MD_SUPERVISED_USER_LEARN_MORE_HTML);
  source->AddResourcePath("supervised_user_learn_more.js",
                          IDR_MD_SUPERVISED_USER_LEARN_MORE_JS);
  source->AddResourcePath("user_manager_styles.html",
                          IDR_MD_USER_MANAGER_STYLES_HTML);
  source->AddResourcePath("user_manager.js", IDR_MD_USER_MANAGER_JS);
  source->AddResourcePath("user_manager_pages.css",
                          IDR_MD_USER_MANAGER_PAGES_CSS);
  source->AddResourcePath("user_manager_pages.html",
                          IDR_MD_USER_MANAGER_PAGES_HTML);
  source->AddResourcePath("user_manager_pages.js",
                          IDR_MD_USER_MANAGER_PAGES_JS);
  source->AddResourcePath("user_manager_tutorial.css",
                          IDR_MD_USER_MANAGER_TUTORIAL_CSS);
  source->AddResourcePath("user_manager_tutorial.html",
                          IDR_MD_USER_MANAGER_TUTORIAL_HTML);
  source->AddResourcePath("user_manager_tutorial.js",
                          IDR_MD_USER_MANAGER_TUTORIAL_JS);
  source->AddResourcePath("shared_styles.html",
                          IDR_MD_USER_MANAGER_SHARED_STYLES_HTML);
  source->AddResourcePath("import_supervised_user.html",
                          IDR_MD_IMPORT_SUPERVISED_USER_HTML);
  source->AddResourcePath("import_supervised_user.js",
                          IDR_MD_IMPORT_SUPERVISED_USER_JS);

  source->SetDefaultResource(IDR_MD_USER_MANAGER_HTML);

  return source;
}

void MDUserManagerUI::GetLocalizedStrings(
    base::DictionaryValue* localized_strings) {
  user_manager_screen_handler_->GetLocalizedValues(localized_strings);
  signin_create_profile_handler_->GetLocalizedValues(localized_strings);
#if defined(ENABLE_SUPERVISED_USERS)
  signin_supervised_user_import_handler_->GetLocalizedValues(localized_strings);
#endif
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings);

#if defined(GOOGLE_CHROME_BUILD)
  localized_strings->SetString("buildType", "chrome");
#else
  localized_strings->SetString("buildType", "chromium");
#endif
}
