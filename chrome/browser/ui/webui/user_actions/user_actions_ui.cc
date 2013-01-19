// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/user_actions/user_actions_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/user_actions/user_actions_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "grit/browser_resources.h"

UserActionsUI::UserActionsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://user-actions/ source.
  content::WebUIDataSource* html_source =
      ChromeWebUIDataSource::Create(chrome::kChromeUIUserActionsHost);
  html_source->SetDefaultResource(IDR_USER_ACTIONS_HTML);
  html_source->AddResourcePath("user_actions.css", IDR_USER_ACTIONS_CSS);
  html_source->AddResourcePath("user_actions.js", IDR_USER_ACTIONS_JS);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddWebUIDataSource(profile, html_source);

  // AddMessageHandler takes ownership of UserActionsUIHandler.
  web_ui->AddMessageHandler(new UserActionsUIHandler());
}

UserActionsUI::~UserActionsUI() {}
