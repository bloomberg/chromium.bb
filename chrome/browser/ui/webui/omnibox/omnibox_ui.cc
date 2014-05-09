// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

OmniboxUI::OmniboxUI(content::WebUI* web_ui) : MojoWebUIController(web_ui) {
  // Set up the chrome://omnibox/ source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOmniboxHost);
  html_source->AddResourcePath("omnibox.css", IDR_OMNIBOX_CSS);
  html_source->AddResourcePath("omnibox.js", IDR_OMNIBOX_JS);
  html_source->SetDefaultResource(IDR_OMNIBOX_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);

  AddMojoResourcePath("chrome/browser/ui/webui/omnibox/omnibox.mojom",
                      IDR_OMNIBOX_MOJO_JS);
}

OmniboxUI::~OmniboxUI() {}

scoped_ptr<MojoWebUIHandler> OmniboxUI::CreateUIHandler(
    mojo::ScopedMessagePipeHandle handle_to_page) {
  return scoped_ptr<MojoWebUIHandler>(
      mojo::BindToPipe(new OmniboxUIHandler(Profile::FromWebUI(web_ui())),
                       handle_to_page.Pass()));
}
