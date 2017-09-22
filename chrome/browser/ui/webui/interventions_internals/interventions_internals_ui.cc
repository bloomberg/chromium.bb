// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

InterventionsInternalsUI::InterventionsInternalsUI(content::WebUI* web_ui)
    : MojoWebUIController(web_ui) {
  // Set up the chrome://interventions-internals/ source.
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIInterventionsInternalsHost);
  source->AddResourcePath("index.js", IDR_INTERVENTIONS_INTERNALS_INDEX_JS);
  source->AddResourcePath(
      "chrome/browser/ui/webui/interventions_internals/"
      "interventions_internals.mojom.js",
      IDR_INTERVENTIONS_INTERNALS_MOJO_INDEX_JS);
  source->SetDefaultResource(IDR_INTERVENTIONS_INTERNALS_INDEX_HTML);
  source->UseGzip(std::vector<std::string>());

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

InterventionsInternalsUI::~InterventionsInternalsUI() {}

void InterventionsInternalsUI::BindUIHandler(
    mojom::InterventionsInternalsPageHandlerRequest request) {
  page_handler_.reset(
      new InterventionsInternalsPageHandler(std::move(request)));
}
