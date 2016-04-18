// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/snippets_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/snippets_internals_message_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

namespace {

content::WebUIDataSource* CreateSnippetsInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISnippetsInternalsHost);

  source->AddResourcePath("snippets_internals.js", IDR_SNIPPETS_INTERNALS_JS);
  source->AddResourcePath("snippets_internals.css", IDR_SNIPPETS_INTERNALS_CSS);
  source->SetDefaultResource(IDR_SNIPPETS_INTERNALS_HTML);
  return source;
}

} // namespace

SnippetsInternalsUI::SnippetsInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateSnippetsInternalsHTMLSource());

  web_ui->AddMessageHandler(new SnippetsInternalsMessageHandler);
}

SnippetsInternalsUI::~SnippetsInternalsUI() {}
