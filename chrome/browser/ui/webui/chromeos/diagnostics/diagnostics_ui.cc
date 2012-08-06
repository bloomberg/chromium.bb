// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/diagnostics/diagnostics_ui.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "chromeos/dbus/debug_daemon_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

namespace chromeos {

namespace {

// JS API callback names.
const char kJsApiUpdateConnStatus[] = "updateConnectivityStatus";

////////////////////////////////////////////////////////////////////////////////
// DiagnosticsHandler

// Class to handle messages from chrome://diagnostics.
class DiagnosticsWebUIHandler : public content::WebUIMessageHandler {
 public:
  DiagnosticsWebUIHandler()
      : weak_ptr_factory_(this) {
  }
  virtual ~DiagnosticsWebUIHandler() {}

 private:
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Called when the page is first loaded.
  void OnPageLoaded(const base::ListValue* args);

  // Called when GetNetworkInterfaces() is complete.
  // |succeeded|: information was obtained successfully.
  // |status|: network interfaces information in json. See
  //      DebugDaemonClient::GetNetworkInterfaces() for details.
  void OnGetNetworkInterfaces(bool succeeded, const std::string& status);

  base::WeakPtrFactory<DiagnosticsWebUIHandler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsWebUIHandler);
};

void DiagnosticsWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "pageLoaded",
      base::Bind(&DiagnosticsWebUIHandler::OnPageLoaded,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DiagnosticsWebUIHandler::OnPageLoaded(const base::ListValue* args) {
  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  DCHECK(debugd_client);

  debugd_client->GetNetworkInterfaces(
      base::Bind(&DiagnosticsWebUIHandler::OnGetNetworkInterfaces,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DiagnosticsWebUIHandler::OnGetNetworkInterfaces(
    bool succeeded, const std::string& status) {
  if (!succeeded)
    return;
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(status));
  if (parsed_value.get() && parsed_value->IsType(Value::TYPE_DICTIONARY)) {
    base::DictionaryValue* result =
        static_cast<DictionaryValue*>(parsed_value.get());
    web_ui()->CallJavascriptFunction(kJsApiUpdateConnStatus, *result);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DiagnosticsUI

DiagnosticsUI::DiagnosticsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new DiagnosticsWebUIHandler());

  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIDiagnosticsHost);
  source->add_resource_path("main.css", IDR_DIAGNOSTICS_MAIN_CSS);
  source->add_resource_path("main.js", IDR_DIAGNOSTICS_MAIN_JS);
  source->set_default_resource(IDR_DIAGNOSTICS_MAIN_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, source);
}

} // namespace chromeos
