// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cryptohome_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/cryptohome_web_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"

namespace chromeos {

namespace {

// Returns HTML data source for chrome://cryptohome.
content::WebUIDataSource* CreateCryptohomeUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUICryptohomeHost);
  source->AddResourcePath("cryptohome.js", IDR_CRYPTOHOME_JS);
  source->SetDefaultResource(IDR_CRYPTOHOME_HTML);
  return source;
}

}  // namespace

CryptohomeUI::CryptohomeUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new CryptohomeWebUIHandler());

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateCryptohomeUIHTMLSource());
}

}  // namespace chromeos
