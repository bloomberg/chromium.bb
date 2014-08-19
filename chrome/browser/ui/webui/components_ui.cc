// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/components_ui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/crx_update_item.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/chromeos/ui_account_tweaks.h"
#endif

using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateComponentsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIComponentsHost);
  source->SetUseJsonJSFormatV2();

  source->AddLocalizedString("componentsTitle", IDS_COMPONENTS_TITLE);
  source->AddLocalizedString("componentsNoneInstalled",
                             IDS_COMPONENTS_NONE_INSTALLED);
  source->AddLocalizedString("componentVersion", IDS_COMPONENTS_VERSION);
  source->AddLocalizedString("checkUpdate", IDS_COMPONENTS_CHECK_FOR_UPDATE);
  source->AddLocalizedString("noComponents", IDS_COMPONENTS_NO_COMPONENTS);
  source->AddLocalizedString("statusLabel", IDS_COMPONENTS_STATUS_LABEL);
  source->AddLocalizedString("checkingLabel", IDS_COMPONENTS_CHECKING_LABEL);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("components.js", IDR_COMPONENTS_JS);
  source->SetDefaultResource(IDR_COMPONENTS_HTML);
#if defined(OS_CHROMEOS)
  chromeos::AddAccountUITweaksLocalizedValues(source, profile);
#endif
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// ComponentsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://components/ page.
class ComponentsDOMHandler : public WebUIMessageHandler {
 public:
  ComponentsDOMHandler();
  virtual ~ComponentsDOMHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Callback for the "requestComponentsData" message.
  void HandleRequestComponentsData(const base::ListValue* args);

  // Callback for the "checkUpdate" message.
  void HandleCheckUpdate(const base::ListValue* args);

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ComponentsDOMHandler);
};

ComponentsDOMHandler::ComponentsDOMHandler() {
}

void ComponentsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestComponentsData",
      base::Bind(&ComponentsDOMHandler::HandleRequestComponentsData,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "checkUpdate",
      base::Bind(&ComponentsDOMHandler::HandleCheckUpdate,
                 base::Unretained(this)));
}

void ComponentsDOMHandler::HandleRequestComponentsData(
    const base::ListValue* args) {
  base::ListValue* list = ComponentsUI::LoadComponents();
  base::DictionaryValue result;
  result.Set("components", list);
  web_ui()->CallJavascriptFunction("returnComponentsData", result);
}

// This function is called when user presses button from html UI.
// TODO(shrikant): We need to make this button available based on current
// state e.g. If component state is currently updating then we need to disable
// button. (https://code.google.com/p/chromium/issues/detail?id=272540)
void ComponentsDOMHandler::HandleCheckUpdate(const base::ListValue* args) {
  if (args->GetSize() != 1) {
    NOTREACHED();
    return;
  }

  std::string component_id;
  if (!args->GetString(0, &component_id)) {
    NOTREACHED();
    return;
  }

  ComponentsUI::OnDemandUpdate(component_id);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// ComponentsUI
//
///////////////////////////////////////////////////////////////////////////////

ComponentsUI::ComponentsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new ComponentsDOMHandler());

  // Set up the chrome://components/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateComponentsUIHTMLSource(profile));
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  cus->AddObserver(this);
}

ComponentsUI::~ComponentsUI() {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (cus)
    cus->RemoveObserver(this);
}

// static
void ComponentsUI::OnDemandUpdate(const std::string& component_id) {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  cus->GetOnDemandUpdater().OnDemandUpdate(component_id);
}

// static
base::ListValue* ComponentsUI::LoadComponents() {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  std::vector<std::string> component_ids;
  component_ids = cus->GetComponentIDs();

  // Construct DictionaryValues to return to UI.
  base::ListValue* component_list = new base::ListValue();
  for (size_t j = 0; j < component_ids.size(); ++j) {
    component_updater::CrxUpdateItem item;
    if (cus->GetComponentDetails(component_ids[j], &item)) {
      base::DictionaryValue* component_entry = new base::DictionaryValue();
      component_entry->SetString("id", component_ids[j]);
      component_entry->SetString("name", item.component.name);
      component_entry->SetString("version", item.component.version.GetString());
      component_entry->SetString("status", ServiceStatusToString(item.status));
      component_list->Append(component_entry);
    }
  }

  return component_list;
}

// static
base::RefCountedMemory* ComponentsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_PLUGINS_FAVICON, scale_factor);
}

base::string16 ComponentsUI::ComponentEventToString(Events event) {
  switch (event) {
    case COMPONENT_UPDATER_STARTED:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_STARTED);
    case COMPONENT_UPDATER_SLEEPING:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_SLEEPING);
    case COMPONENT_UPDATE_FOUND:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_FOUND);
    case COMPONENT_UPDATE_READY:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_READY);
    case COMPONENT_UPDATED:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_UPDATED);
    case COMPONENT_NOT_UPDATED:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_NOTUPDATED);
    case COMPONENT_UPDATE_DOWNLOADING:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_DOWNLOADING);
  }
  return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
}

base::string16 ComponentsUI::ServiceStatusToString(
    component_updater::CrxUpdateItem::Status status) {
  switch (status) {
    case component_updater::CrxUpdateItem::kNew:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_NEW);
    case component_updater::CrxUpdateItem::kChecking:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_CHECKING);
    case component_updater::CrxUpdateItem::kCanUpdate:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATE);
    case component_updater::CrxUpdateItem::kDownloadingDiff:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_DNL_DIFF);
    case component_updater::CrxUpdateItem::kDownloading:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_DNL);
    case component_updater::CrxUpdateItem::kUpdatingDiff:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDT_DIFF);
    case component_updater::CrxUpdateItem::kUpdating:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATING);
    case component_updater::CrxUpdateItem::kUpdated:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATED);
    case component_updater::CrxUpdateItem::kUpToDate:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPTODATE);
    case component_updater::CrxUpdateItem::kNoUpdate:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_NOUPDATE);
    case component_updater::CrxUpdateItem::kLastStatus:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
  }
  return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
}

void ComponentsUI::OnEvent(Events event, const std::string& id) {
  base::DictionaryValue parameters;
  parameters.SetString("event", ComponentEventToString(event));
  if (!id.empty()) {
    using component_updater::ComponentUpdateService;
    if (event == ComponentUpdateService::Observer::COMPONENT_UPDATED) {
      ComponentUpdateService* cus = g_browser_process->component_updater();
      component_updater::CrxUpdateItem item;
      if (cus->GetComponentDetails(id, &item))
        parameters.SetString("version", item.component.version.GetString());
    }
    parameters.SetString("id", id);
  }
  web_ui()->CallJavascriptFunction("onComponentEvent", parameters);
}
