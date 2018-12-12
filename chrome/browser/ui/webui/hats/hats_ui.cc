// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hats/hats_ui.h"

#include <utility>

#include "base/optional.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/hats/hats_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

HatsUI::HatsUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  // Set up the chrome://hats/ source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIHatsHost);

  // TODO(jeffreycohen) The Site ID for the current survey will need to be
  // passed into the JS with code similar to this :
  // source->AddString("trigger", "z4cctguzopq5x2ftal6vdgjrui");
  source->SetJsonPath("strings.js");

  source->AddResourcePath("hats.js", IDR_DESKTOP_HATS_JS);
  source->SetDefaultResource(IDR_DESKTOP_HATS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);

  auto handler = std::make_unique<HatsHandler>();
  web_ui->AddMessageHandler(std::move(handler));
}

HatsUI::~HatsUI() {}
