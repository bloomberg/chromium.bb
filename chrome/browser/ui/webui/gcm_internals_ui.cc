// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/gcm_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

GCMInternalsUI::GCMInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://gcm-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIGCMInternalsHost);
  html_source->SetUseJsonJSFormatV2();

  html_source->SetJsonPath("strings.js");

  // Add required resources.
  html_source->AddResourcePath("gcm_internals.css", IDR_GCM_INTERNALS_CSS);
  html_source->AddResourcePath("gcm_internals.js", IDR_GCM_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_GCM_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, html_source);
}

GCMInternalsUI::~GCMInternalsUI() {}
