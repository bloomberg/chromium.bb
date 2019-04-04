// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace settings {

OSSettingsUI::OSSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);

  // TODO(jamescook): Factor out page handlers from MdSettingsUI and initialize
  // them both there and here.

  html_source->UseGzip();
  html_source->SetDefaultResource(IDR_OS_SETTINGS_HTML);

  content::WebUIDataSource::Add(profile, html_source);
}

OSSettingsUI::~OSSettingsUI() = default;

}  // namespace settings
}  // namespace chromeos
