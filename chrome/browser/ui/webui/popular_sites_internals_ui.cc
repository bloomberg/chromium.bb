// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/popular_sites_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/popular_sites_internals_message_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

content::WebUIDataSource* CreatePopularSitesInternalsHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIPopularSitesInternalsHost);

  source->AddResourcePath("popular_sites_internals.js",
                          IDR_POPULAR_SITES_INTERNALS_JS);
  source->AddResourcePath("popular_sites_internals.css",
                          IDR_POPULAR_SITES_INTERNALS_CSS);
  source->SetDefaultResource(IDR_POPULAR_SITES_INTERNALS_HTML);
  return source;
}

PopularSitesInternalsUI::PopularSitesInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreatePopularSitesInternalsHTMLSource());

  web_ui->AddMessageHandler(new PopularSitesInternalsMessageHandler);
}

PopularSitesInternalsUI::~PopularSitesInternalsUI() {}
