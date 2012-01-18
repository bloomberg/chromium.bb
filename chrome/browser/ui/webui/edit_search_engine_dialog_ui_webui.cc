// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/edit_search_engine_dialog_ui_webui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

EditSearchEngineDialogUI::EditSearchEngineDialogUI(content::WebUI* web_ui)
    : HtmlDialogUI(web_ui) {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIEditSearchEngineDialogHost);

  source->AddLocalizedString("titleNew",
                             IDS_SEARCH_ENGINES_EDITOR_NEW_WINDOW_TITLE);
  source->AddLocalizedString("titleEdit",
                             IDS_SEARCH_ENGINES_EDITOR_EDIT_WINDOW_TITLE);
  source->AddLocalizedString("descriptionLabel",
                             IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_LABEL);
  source->AddLocalizedString("keywordLabel",
                             IDS_SEARCH_ENGINES_EDITOR_KEYWORD_LABEL);
  source->AddLocalizedString("urlLabel", IDS_SEARCH_ENGINES_EDITOR_URL_LABEL);
  source->AddLocalizedString("urlDescriptionLabel",
                             IDS_SEARCH_ENGINES_EDITOR_URL_DESCRIPTION_LABEL);
  source->AddLocalizedString("invalidTitle",
                             IDS_SEARCH_ENGINES_INVALID_TITLE_TT);
  source->AddLocalizedString("invalidKeyword",
                             IDS_SEARCH_ENGINES_INVALID_KEYWORD_TT);
  source->AddLocalizedString("invalidUrl", IDS_SEARCH_ENGINES_INVALID_URL_TT);
  source->AddLocalizedString("cancel", IDS_CANCEL);
  source->AddLocalizedString("save", IDS_SAVE);

  // Set the json path.
  source->set_json_path("strings.js");

  // Add required resources.
  source->add_resource_path("edit_search_engine_dialog.js",
                            IDR_EDIT_SEARCH_ENGINE_DIALOG_JS);
  source->add_resource_path("edit_search_engine_dialog.css",
                            IDR_EDIT_SEARCH_ENGINE_DIALOG_CSS);

  // Set default resource.
  source->set_default_resource(IDR_EDIT_SEARCH_ENGINE_DIALOG_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  profile->GetChromeURLDataManager()->AddDataSource(source);
}

EditSearchEngineDialogUI::~EditSearchEngineDialogUI() {
}
