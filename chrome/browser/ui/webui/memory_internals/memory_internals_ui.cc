// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/memory_internals/memory_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/memory_internals/memory_internals_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/memory_internals_resources.h"

namespace {

content::WebUIDataSource* CreateMemoryInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIMemoryInternalsHost);

  source->AddResourcePath("memory_internals.js",
                          IDR_MEMORY_INTERNALS_MEMORY_INTERNALS_JS);
  source->SetDefaultResource(IDR_MEMORY_INTERNALS_MEMORY_INTERNALS_HTML);
  return source;
}

}  // namespace

MemoryInternalsUI::MemoryInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new MemoryInternalsHandler());

  // Set up the chrome://memory-internals/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateMemoryInternalsHTMLSource());
}
