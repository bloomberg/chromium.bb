// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cryptohome_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/chromeos/cryptohome_web_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"

namespace chromeos {

namespace {

// Returns HTML data source for chrome://cryptohome.
ChromeWebUIDataSource* CreateCryptohomeUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUICryptohomeHost);
  source->add_resource_path("cryptohome.js", IDR_CRYPTOHOME_JS);
  source->set_default_resource(IDR_CRYPTOHOME_HTML);
  return source;
}

}  // namespace

CryptohomeUI::CryptohomeUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new CryptohomeWebUIHandler());

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(profile,
                                          CreateCryptohomeUIHTMLSource());
}

}  // namespace chromeos
