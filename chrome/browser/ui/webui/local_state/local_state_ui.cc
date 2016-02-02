// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_state/local_state_ui.h"

#include <string>

#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

namespace {

// UI Handler for chrome://local-state. Displays the Local State file as JSON.
class LocalStateUIHandler : public content::WebUIMessageHandler {
 public:
  LocalStateUIHandler();
  ~LocalStateUIHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // Called from JS when the page has loaded. Serializes local state prefs and
  // sends them to the page.
  void HandleRequestJson(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(LocalStateUIHandler);
};

LocalStateUIHandler::LocalStateUIHandler() {
}

LocalStateUIHandler::~LocalStateUIHandler() {
}

void LocalStateUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestJson", base::Bind(&LocalStateUIHandler::HandleRequestJson,
                                base::Unretained(this)));
}

void LocalStateUIHandler::HandleRequestJson(const base::ListValue* args) {
#if !defined(OS_CHROMEOS)
  scoped_ptr<base::DictionaryValue> local_state_values(
      g_browser_process->local_state()->GetPreferenceValuesOmitDefaults());

  std::string json;
  JSONStringValueSerializer serializer(&json);
  serializer.set_pretty_print(true);
  bool result = serializer.Serialize(*local_state_values);
  if (!result)
    json = "Error loading Local State file.";

  web_ui()->CallJavascriptFunction("localState.setLocalState",
                                   base::StringValue(json));
#endif  // !defined(OS_CHROMEOS)
}

}  // namespace

LocalStateUI::LocalStateUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://local-state source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUILocalStateHost);
  html_source->SetDefaultResource(IDR_LOCAL_STATE_HTML);
  html_source->AddResourcePath("local_state.js", IDR_LOCAL_STATE_JS);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);

  // AddMessageHandler takes ownership of LocalStateUIHandler.
  web_ui->AddMessageHandler(new LocalStateUIHandler);
}

LocalStateUI::~LocalStateUI() {
}
