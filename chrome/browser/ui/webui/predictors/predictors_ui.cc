// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/predictors/predictors_ui.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/predictors/predictors_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

content::WebUIDataSource* CreatePredictorsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPredictorsHost);
  source->AddResourcePath("predictors.js", IDR_PREDICTORS_JS);
  source->SetDefaultResource(IDR_PREDICTORS_HTML);
  source->UseGzip(std::unordered_set<std::string>());
  return source;
}

}  // namespace

PredictorsUI::PredictorsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(base::MakeUnique<PredictorsHandler>(profile));
  content::WebUIDataSource::Add(profile, CreatePredictorsUIHTMLSource());
}
