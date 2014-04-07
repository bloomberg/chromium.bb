// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_info_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace extensions {

ExtensionInfoUI::ExtensionInfoUI(content::WebUI* web_ui, const GURL& url)
    : content::WebUIController(web_ui),
      source_(content::WebUIDataSource::Create(
          chrome::kChromeUIExtensionInfoHost)) {
  AddExtensionDataToSource(url.path().substr(1));

  source_->AddLocalizedString("isRunning",
                              IDS_EXTENSION_SCRIPT_POPUP_IS_RUNNING);
  source_->AddLocalizedString("lastUpdated",
                              IDS_EXTENSION_SCRIPT_POPUP_LAST_UPDATED);
  source_->SetUseJsonJSFormatV2();
  source_->SetJsonPath("strings.js");

  source_->AddResourcePath("extension_info.css", IDR_EXTENSION_INFO_CSS);
  source_->AddResourcePath("extension_info.js", IDR_EXTENSION_INFO_JS);
  source_->SetDefaultResource(IDR_EXTENSION_INFO_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source_);
}

ExtensionInfoUI::~ExtensionInfoUI() {
}

// static
GURL ExtensionInfoUI::GetURL(const std::string& extension_id) {
  return GURL(base::StringPrintf(
      "%s%s", chrome::kChromeUIExtensionInfoURL, extension_id.c_str()));
}

void ExtensionInfoUI::AddExtensionDataToSource(
    const std::string& extension_id) {
  Profile* profile = Profile::FromWebUI(web_ui());
  ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();
  const Extension* extension =
      extension_service->extensions()->GetByID(extension_id);
  if (!extension)
    return;

  base::DictionaryValue extension_data;
  GetExtensionBasicInfo(extension, true, &extension_data);
  source_->AddLocalizedStrings(extension_data);

  // Set the icon URL.
  GURL icon =
      ExtensionIconSource::GetIconURL(extension,
                                      extension_misc::EXTENSION_ICON_MEDIUM,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      false, NULL);
  source_->AddString("icon", base::UTF8ToUTF16(icon.spec()));
  // Set the last update time (the install time).
  base::Time install_time =
      ExtensionPrefs::Get(profile)->GetInstallTime(extension_id);
  source_->AddString("installTime", base::TimeFormatShortDate(install_time));
}

}  // namespace extensions
