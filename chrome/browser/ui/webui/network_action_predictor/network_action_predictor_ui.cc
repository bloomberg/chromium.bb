// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/network_action_predictor/network_action_predictor_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/network_action_predictor/network_action_predictor_dom_handler.h"
#include "chrome/common/url_constants.h"
#include "content/browser/webui/web_ui.h"
#include "content/public/browser/web_contents.h"
#include "grit/browser_resources.h"

namespace {

ChromeWebUIDataSource* CreateNetworkActionPredictorUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUINetworkActionPredictorHost);
  source->add_resource_path("network_action_predictor.js",
                            IDR_NETWORK_ACTION_PREDICTOR_JS);
  source->set_default_resource(IDR_NETWORK_ACTION_PREDICTOR_HTML);
  return source;
}

}  // namespace

NetworkActionPredictorUI::NetworkActionPredictorUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromBrowserContext(
      web_ui->web_contents()->GetBrowserContext());
  web_ui->AddMessageHandler(new NetworkActionPredictorDOMHandler(profile));
  profile->GetChromeURLDataManager()->AddDataSource(
      CreateNetworkActionPredictorUIHTMLSource());
}
