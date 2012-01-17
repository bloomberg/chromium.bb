// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hung_renderer_dialog_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/url_constants.h"
#include "content/browser/webui/web_ui.h"
#include "content/public/browser/web_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

HungRendererDialogUI::HungRendererDialogUI(WebUI* web_ui)
    : HtmlDialogUI(web_ui) {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIHungRendererDialogHost);

  source->AddLocalizedString("title", IDS_BROWSER_HANGMONITOR_RENDERER_TITLE);
  source->AddLocalizedString("explanation", IDS_BROWSER_HANGMONITOR_RENDERER);
  source->AddLocalizedString("kill", IDS_BROWSER_HANGMONITOR_RENDERER_END);
  source->AddLocalizedString("wait", IDS_BROWSER_HANGMONITOR_RENDERER_WAIT);

  // Set the json path.
  source->set_json_path("strings.js");

  // Add required resources.
  source->add_resource_path("hung_renderer_dialog.js",
                            IDR_HUNG_RENDERER_DIALOG_JS);
  source->add_resource_path("hung_renderer_dialog.css",
                            IDR_HUNG_RENDERER_DIALOG_CSS);

  // Set default resource.
  source->set_default_resource(IDR_HUNG_RENDERER_DIALOG_HTML);

  Profile* profile = Profile::FromBrowserContext(
      web_ui->web_contents()->GetBrowserContext());
  profile->GetChromeURLDataManager()->AddDataSource(source);

  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(theme);
}

HungRendererDialogUI::~HungRendererDialogUI() {
}
