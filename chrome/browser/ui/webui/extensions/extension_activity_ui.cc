// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_activity_ui.h"

#include "base/bind.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/webui/shared_resources_data_source.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

ExtensionActivityUI::ExtensionActivityUI(content::WebUI* web_ui)
    : WebUIController(web_ui), extension_(NULL) {
  web_ui->HideURL();
  web_ui->OverrideTitle(l10n_util::GetStringUTF16(
      IDS_EXTENSION_ACTIVITY_TITLE));

  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIExtensionActivityHost);

  // Localized strings.
  source->AddLocalizedString("extensionActivity", IDS_EXTENSION_ACTIVITY_TITLE);
  source->AddLocalizedString("extensionActivityApiCall",
                             IDS_EXTENSION_ACTIVITY_API_CALL);
  source->AddLocalizedString("extensionActivityApiBlock",
                             IDS_EXTENSION_ACTIVITY_API_BLOCK);
  source->AddLocalizedString("extensionActivityContentScript",
                             IDS_EXTENSION_ACTIVITY_CONTENT_SCRIPT);
  source->set_use_json_js_format_v2();
  source->set_json_path("strings.js");

  // Resources.
  source->add_resource_path("extension_activity.js", IDR_EXTENSION_ACTIVITY_JS);
  source->set_default_resource(IDR_EXTENSION_ACTIVITY_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(profile, source);
  ChromeURLDataManager::AddDataSource(profile, new SharedResourcesDataSource());

  // Callback handlers.
  web_ui->RegisterMessageCallback("requestExtensionData",
      base::Bind(&ExtensionActivityUI::HandleRequestExtensionData,
                 base::Unretained(this)));
}

ExtensionActivityUI::~ExtensionActivityUI() {
  if (extension_)
    extensions::ActivityLog::GetInstance()->RemoveObserver(extension_, this);
}

void ExtensionActivityUI::HandleRequestExtensionData(
    const base::ListValue* args) {
  DCHECK_EQ(1U, args->GetSize());

  std::string extension_id;
  if (!args->GetString(0, &extension_id))
    return;

  ExtensionService* extension_service = Profile::FromWebUI(web_ui())->
      GetExtensionService();
  extension_ = extension_service->GetExtensionById(extension_id, false);
  if (!extension_)
    return;

  GURL icon =
      ExtensionIconSource::GetIconURL(extension_,
                                      extension_misc::EXTENSION_ICON_MEDIUM,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      false, NULL);

  DictionaryValue* extension_data = new DictionaryValue();  // Owned by result.
  extension_data->SetString("id", extension_->id());
  extension_data->SetString("name", extension_->name());
  extension_data->SetString("version", extension_->version()->GetString());
  extension_data->SetString("description", extension_->description());
  extension_data->SetString("icon", icon.spec());

  DictionaryValue result;
  result.Set("extension", extension_data);

  web_ui()->CallJavascriptFunction("extension_activity.handleExtensionData",
                                   result);

  extensions::ActivityLog::GetInstance()->AddObserver(extension_, this);
}

void ExtensionActivityUI::OnExtensionActivity(
      const extensions::Extension* extension,
      extensions::ActivityLog::Activity activity,
      const std::vector<std::string>& messages) {
  scoped_ptr<ListValue> messages_list(new ListValue());
  messages_list->AppendStrings(messages);

  DictionaryValue result;
  result.SetInteger("activity", activity);
  result.Set("messages", messages_list.release());

  web_ui()->CallJavascriptFunction("extension_activity.handleExtensionActivity",
                                   result);
}
