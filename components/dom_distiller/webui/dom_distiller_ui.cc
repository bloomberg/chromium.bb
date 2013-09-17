// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/webui/dom_distiller_ui.h"

#include "components/dom_distiller/core/dom_distiller_constants.h"
#include "components/dom_distiller/webui/dom_distiller_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/component_strings.h"
#include "grit/dom_distiller_resources.h"

namespace dom_distiller {

DomDistillerUI::DomDistillerUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up WebUIDataSource.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIDomDistillerHost);
  source->SetDefaultResource(IDR_ABOUT_DOM_DISTILLER_HTML);
  source->AddResourcePath("about_dom_distiller.css",
                          IDR_ABOUT_DOM_DISTILLER_CSS);
  source->AddResourcePath("about_dom_distiller.js",
                          IDR_ABOUT_DOM_DISTILLER_JS);

  source->SetUseJsonJSFormatV2();
  source->AddLocalizedString("domDistillerTitle", IDS_DOM_DISTILLER_TITLE);
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source);
  source->SetJsonPath("strings.js");

  // Add message handler.
  web_ui->AddMessageHandler(new DomDistillerHandler());
}

DomDistillerUI::~DomDistillerUI() {}

}  // namespace dom_distiller
