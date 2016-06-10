// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/hats/hats_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

HatsUI::HatsUI(content::WebUI* web_ui)
    : WebDialogUI(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIHatsHost);

  source->SetDefaultResource(IDR_HATS_HTML);
  source->DisableContentSecurityPolicy();

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

HatsUI::~HatsUI() {}

}  // namespace chromeos
