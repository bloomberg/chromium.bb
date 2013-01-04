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
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace chromeos {

namespace {

// JS API callback names.
const char kJsApiSetNetifStatus[] = "diag.DiagPage.setNetifStatus";
const char kJsApiSetTestICMPStatus[] = "diag.DiagPage.setTestICMPStatus";

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

  // Called by JS layer to get network interfaces status.
  void GetNetworkInterfaces(const base::ListValue* args);

  // Called by JS layer to test ICMP connectivity to a specified host.
  void TestICMP(const base::ListValue* args);

  // Called when GetNetworkInterfaces() is complete.
  // |succeeded|: information was obtained successfully.
  // |status|: network interfaces information in json. See
  //      DebugDaemonClient::GetNetworkInterfaces() for details.
  void OnGetNetworkInterfaces(bool succeeded, const std::string& status);

  // Called when TestICMP() is complete.
  // |succeeded|: information was obtained successfully.
  // |status|: information about ICMP connectivity in json. See
  //      DebugDaemonClient::TestICMP() for details.
  void OnTestICMP(bool succeeded, const std::string& status);

  base::WeakPtrFactory<DiagnosticsWebUIHandler> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DiagnosticsWebUIHandler);
};

void DiagnosticsWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getNetworkInterfaces",
      base::Bind(&DiagnosticsWebUIHandler::GetNetworkInterfaces,
                 weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "testICMP",
      base::Bind(&DiagnosticsWebUIHandler::TestICMP,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DiagnosticsWebUIHandler::GetNetworkInterfaces(
    const base::ListValue* args) {
  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  DCHECK(debugd_client);

  debugd_client->GetNetworkInterfaces(
      base::Bind(&DiagnosticsWebUIHandler::OnGetNetworkInterfaces,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DiagnosticsWebUIHandler::TestICMP(const base::ListValue* args) {
  chromeos::DebugDaemonClient* debugd_client =
      chromeos::DBusThreadManager::Get()->GetDebugDaemonClient();
  DCHECK(debugd_client);
  DCHECK(args);
  DCHECK_EQ(1u, args->GetSize());
  if (!args || args->GetSize() != 1)
    return;

  std::string host_address;
  if (!args->GetString(0, &host_address))
    return;

  debugd_client->TestICMP(host_address,
                          base::Bind(&DiagnosticsWebUIHandler::OnTestICMP,
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

void DiagnosticsWebUIHandler::OnTestICMP(
    bool succeeded, const std::string& status) {
  if (!succeeded)
    return;
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(status));
  if (parsed_value.get() && parsed_value->IsType(Value::TYPE_DICTIONARY)) {
    base::DictionaryValue* result =
        static_cast<DictionaryValue*>(parsed_value.get());
    web_ui()->CallJavascriptFunction(kJsApiSetTestICMPStatus, *result);
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
  source->AddLocalizedString("refresh", IDS_DIAGNOSTICS_REFRESH);
  source->AddLocalizedString("choose-adapter", IDS_DIAGNOSTICS_CHOOSE_ADAPTER);
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
  source->AddLocalizedString("gateway-not-connected-to-internet",
                             IDS_DIAGNOSTICS_GATEWAY_NOT_CONNECTED_TO_INTERNET);
  source->AddLocalizedString("enable-adapter",
                             IDS_DIAGNOSTICS_ENABLE_ADAPTER);
  source->AddLocalizedString("fix-no-ip-wifi",
                             IDS_DIAGNOSTICS_FIX_NO_IP_WIFI);
  source->AddLocalizedString("fix-no-ip-ethernet",
                             IDS_DIAGNOSTICS_FIX_NO_IP_ETHERNET);
  source->AddLocalizedString("fix-no-ip-3g",
                             IDS_DIAGNOSTICS_FIX_NO_IP_3G);
  source->AddLocalizedString("fix-gateway-connection",
                             IDS_DIAGNOSTICS_FIX_GATEWAY_CONNECTION);
  source->set_default_resource(IDR_DIAGNOSTICS_MAIN_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, source);
}

}  // namespace chromeos
