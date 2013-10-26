// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/components_ui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
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

content::WebUIDataSource* CreateComponentsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIComponentsHost);
  source->SetUseJsonJSFormatV2();

  source->AddLocalizedString("componentsTitle", IDS_COMPONENTS_TITLE);
  source->AddLocalizedString("componentsNoneInstalled",
                             IDS_COMPONENTS_NONE_INSTALLED);
  source->AddLocalizedString("componentVersion", IDS_COMPONENTS_VERSION);
  source->AddLocalizedString("checkUpdate", IDS_COMPONENTS_CHECK_FOR_UPDATE);
  source->AddLocalizedString("noComponents", IDS_COMPONENTS_NO_COMPONENTS);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("components.js", IDR_COMPONENTS_JS);
  source->SetDefaultResource(IDR_COMPONENTS_HTML);
#if defined(OS_CHROMEOS)
  chromeos::AddAccountUITweaksLocalizedValues(source);
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
  void HandleRequestComponentsData(const ListValue* args);

  // Callback for the "checkUpdate" message.
  void HandleCheckUpdate(const ListValue* args);

 private:
  void LoadComponents();

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ComponentsDOMHandler);
};

ComponentsDOMHandler::ComponentsDOMHandler() {
}

void ComponentsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestComponentsData",
      base::Bind(&ComponentsDOMHandler::HandleRequestComponentsData,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("checkUpdate",
      base::Bind(&ComponentsDOMHandler::HandleCheckUpdate,
                 base::Unretained(this)));
}

void ComponentsDOMHandler::HandleRequestComponentsData(const ListValue* args) {
  LoadComponents();
}

// This function is called when user presses button from html UI.
// TODO(shrikant): We need to make this button available based on current
// state e.g. If component state is currently updating then we need to disable
// button. (https://code.google.com/p/chromium/issues/detail?id=272540)
void ComponentsDOMHandler::HandleCheckUpdate(const ListValue* args) {
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

void ComponentsDOMHandler::LoadComponents() {
  ComponentUpdateService* cus = g_browser_process->component_updater();
  std::vector<CrxComponentInfo> components;
  cus->GetComponents(&components);

  // Construct DictionaryValues to return to UI.
  ListValue* component_list = new ListValue();
  for (size_t j = 0; j < components.size(); ++j) {
    const CrxComponentInfo& component = components[j];

    DictionaryValue* component_entry = new DictionaryValue();
    component_entry->SetString("id", component.id);
    component_entry->SetString("name", component.name);
    component_entry->SetString("version", component.version);

    component_list->Append(component_entry);
  }

  DictionaryValue results;
  results.Set("components", component_list);
  web_ui()->CallJavascriptFunction("returnComponentsData", results);
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
  content::WebUIDataSource::Add(profile, CreateComponentsUIHTMLSource());
}

// static
void ComponentsUI::OnDemandUpdate(const std::string& component_id) {
  ComponentUpdateService* cus = g_browser_process->component_updater();
  cus->OnDemandUpdate(component_id);
}

// static
base::RefCountedMemory* ComponentsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ResourceBundle::GetSharedInstance().
      LoadDataResourceBytesForScale(IDR_PLUGINS_FAVICON, scale_factor);
}
