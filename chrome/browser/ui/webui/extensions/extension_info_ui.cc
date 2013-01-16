// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_info_ui.h"

#include "base/i18n/time_formatting.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

ExtensionInfoUI::ExtensionInfoUI(content::WebUI* web_ui, const GURL& url)
    : content::WebUIController(web_ui),
      source_(new ChromeWebUIDataSource(chrome::kChromeUIExtensionInfoHost)) {
  AddExtensionDataToSource(url.path().substr(1));

  source_->AddLocalizedString("isRunning",
                              IDS_EXTENSION_SCRIPT_POPUP_IS_RUNNING);
  source_->AddLocalizedString("lastUpdated",
                              IDS_EXTENSION_SCRIPT_POPUP_LAST_UPDATED);
  source_->set_use_json_js_format_v2();
  source_->set_json_path("strings.js");

  source_->add_resource_path("extension_info.css", IDR_EXTENSION_INFO_CSS);
  source_->add_resource_path("extension_info.js", IDR_EXTENSION_INFO_JS);
  source_->set_default_resource(IDR_EXTENSION_INFO_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(profile, source_);
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
  ExtensionService* extension_service = extensions::ExtensionSystem::Get(
      Profile::FromWebUI(web_ui()))->extension_service();
  const extensions::Extension* extension =
      extension_service->extensions()->GetByID(extension_id);
  if (!extension)
    return;

  extension->GetBasicInfo(true, source_->localized_strings());

  // Set the icon URL.
  GURL icon =
      ExtensionIconSource::GetIconURL(extension,
                                      extension_misc::EXTENSION_ICON_MEDIUM,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      false, NULL);
  source_->AddString("icon", UTF8ToUTF16(icon.spec()));
  // Set the last update time (the install time).
  base::Time install_time = extension_service->extension_prefs()->
      GetInstallTime(extension_id);
  source_->AddString("installTime", base::TimeFormatShortDate(install_time));
}
