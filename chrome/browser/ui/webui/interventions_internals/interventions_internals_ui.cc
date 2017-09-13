// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

content::WebUIDataSource* CreateInterventionsInternalsHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIInterventionsInternalsHost);

  source->SetDefaultResource(IDR_INTERVENTIONS_INTERNALS_INDEX_HTML);
  source->UseGzip(std::vector<std::string>());
  return source;
}

}  // namespace

InterventionsInternalsUI::InterventionsInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://interventions-internals/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreateInterventionsInternalsHTMLSource());

  // TODO(thanhdle): Add a previews message handler.
  // crbug.com/764409
}
