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
#include "grit/generated_resources.h"

namespace chromeos {

namespace {

// JS API callback names.
const char kJsApiSetNetifStatus[] = "diag.DiagPage.setNetifStatus";

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
    web_ui()->CallJavascriptFunction(kJsApiSetNetifStatus, *result);
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
  source->set_json_path("strings.js");
  source->add_resource_path("main.css", IDR_DIAGNOSTICS_MAIN_CSS);
  source->add_resource_path("main.js", IDR_DIAGNOSTICS_MAIN_JS);
  source->add_resource_path("fail.png", IDR_DIAGNOSTICS_IMAGES_FAIL);
  source->add_resource_path("tick.png", IDR_DIAGNOSTICS_IMAGES_TICK);
  source->add_resource_path("warning.png", IDR_DIAGNOSTICS_IMAGES_WARNING);
  source->AddLocalizedString("diagnostics", IDS_DIAGNOSTICS_DIAGNOSTICS_TITLE);
  source->AddLocalizedString("connectivity",
                             IDS_DIAGNOSTICS_CONNECTIVITY_TITLE);
  source->AddLocalizedString("loading", IDS_DIAGNOSTICS_LOADING);
  source->AddLocalizedString("wlan0", IDS_DIAGNOSTICS_ADAPTER_WLAN0);
  source->AddLocalizedString("eth0", IDS_DIAGNOSTICS_ADAPTER_ETH0);
  source->AddLocalizedString("eth1", IDS_DIAGNOSTICS_ADAPTER_ETH1);
  source->AddLocalizedString("wwan0", IDS_DIAGNOSTICS_ADAPTER_WWAN0);
  source->AddLocalizedString("testing-hardware",
                             IDS_DIAGNOSTICS_TESTING_HARDWARE);
  source->AddLocalizedString("testing-connection-to-router",
                             IDS_DIAGNOSTICS_TESTING_CONNECTION_TO_ROUTER);
  source->AddLocalizedString("testing-connection-to-internet",
                             IDS_DIAGNOSTICS_TESTING_CONNECTION_TO_INTERNET);
  source->AddLocalizedString("adapter-disabled",
                             IDS_DIAGNOSTICS_ADAPTER_DISABLED);
  source->AddLocalizedString("adapter-no-ip",
                             IDS_DIAGNOSTICS_ADAPTER_NO_IP);
  source->AddLocalizedString("enable-adapter",
                             IDS_DIAGNOSTICS_ENABLE_ADAPTER);
  source->AddLocalizedString("fix-connection-to-router",
                             IDS_DIAGNOSTICS_FIX_CONNECTION_TO_ROUTER);
  source->set_default_resource(IDR_DIAGNOSTICS_MAIN_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, source);
}

} // namespace chromeos
