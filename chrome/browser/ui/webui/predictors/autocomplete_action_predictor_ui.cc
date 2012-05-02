// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/predictors/autocomplete_action_predictor_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/predictors/autocomplete_action_predictor_dom_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"

namespace {

ChromeWebUIDataSource* CreateAutocompleteActionPredictorUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIPredictorsHost);
  source->add_resource_path("autocomplete_action_predictor.js",
                            IDR_AUTOCOMPLETE_ACTION_PREDICTOR_JS);
  source->set_default_resource(IDR_AUTOCOMPLETE_ACTION_PREDICTOR_HTML);
  return source;
}

}  // namespace

AutocompleteActionPredictorUI::AutocompleteActionPredictorUI(
    content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(new AutocompleteActionPredictorDOMHandler(profile));
  ChromeURLDataManager::AddDataSource(profile,
      CreateAutocompleteActionPredictorUIHTMLSource());
}
