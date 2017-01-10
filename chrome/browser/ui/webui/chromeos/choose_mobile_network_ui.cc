// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/choose_mobile_network_ui.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

// JS API callbacks names.
const char kJsApiCancel[] = "cancel";
const char kJsApiConnect[] = "connect";
const char kJsApiPageReady[] = "pageReady";

// Page JS API function names.
const char kJsApiShowNetworks[] = "mobile.ChooseNetwork.showNetworks";
const char kJsApiShowScanning[] = "mobile.ChooseNetwork.showScanning";

// Network properties.
const char kNetworkIdProperty[] = "networkId";
const char kOperatorNameProperty[] = "operatorName";
const char kStatusProperty[] = "status";
const char kTechnologyProperty[] = "technology";

content::WebUIDataSource* CreateChooseMobileNetworkUIHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIChooseMobileNetworkHost);

  source->AddLocalizedString("chooseNetworkTitle",
                             IDS_NETWORK_CHOOSE_MOBILE_NETWORK);
  source->AddLocalizedString("scanningMsgLine1",
                             IDS_NETWORK_SCANNING_FOR_MOBILE_NETWORKS);
  source->AddLocalizedString("scanningMsgLine2",
                             IDS_NETWORK_SCANNING_THIS_MAY_TAKE_A_MINUTE);
  source->AddLocalizedString("noMobileNetworks",
                             IDS_NETWORK_NO_MOBILE_NETWORKS);
  source->AddLocalizedString("connect", IDS_OPTIONS_SETTINGS_CONNECT);
  source->AddLocalizedString("cancel", IDS_CANCEL);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("choose_mobile_network.js",
                          IDR_CHOOSE_MOBILE_NETWORK_JS);
  source->SetDefaultResource(IDR_CHOOSE_MOBILE_NETWORK_HTML);
  return source;
}

chromeos::NetworkDeviceHandler* GetNetworkDeviceHandler() {
  return chromeos::NetworkHandler::Get()->network_device_handler();
}

chromeos::NetworkStateHandler* GetNetworkStateHandler() {
  return chromeos::NetworkHandler::Get()->network_state_handler();
}

void NetworkOperationErrorCallback(
    const std::string& operation_name,
    const std::string& error_name,
    std::unique_ptr<base::DictionaryValue> error_data) {
  NET_LOG_ERROR("Operation failed: " + error_name, operation_name);
}

class ChooseMobileNetworkHandler
    : public WebUIMessageHandler,
      public NetworkStateHandlerObserver {
 public:
  ChooseMobileNetworkHandler();
  ~ChooseMobileNetworkHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // NetworkStateHandlerObserver implementation.
  void DeviceListChanged() override;

 private:
  // Handlers for JS WebUI messages.
  void HandleCancel(const base::ListValue* args);
  void HandleConnect(const base::ListValue* args);
  void HandlePageReady(const base::ListValue* args);

  std::string device_path_;
  base::ListValue networks_list_;
  bool is_page_ready_;
  bool scanning_;
  bool has_pending_results_;

  DISALLOW_COPY_AND_ASSIGN(ChooseMobileNetworkHandler);
};

// ChooseMobileNetworkHandler implementation.

ChooseMobileNetworkHandler::ChooseMobileNetworkHandler()
    : is_page_ready_(false),
      scanning_(false),
      has_pending_results_(false) {
  NetworkStateHandler* handler = GetNetworkStateHandler();
  const DeviceState* cellular =
      handler->GetDeviceStateByType(NetworkTypePattern::Cellular());
  if (!cellular) {
    NET_LOG_ERROR(
        "A cellular device is not available.",
        "Cannot initiate a cellular network scan without a cellular device.");
    return;
  }
  handler->AddObserver(this, FROM_HERE);
  device_path_ = cellular->path();
  GetNetworkDeviceHandler()->ProposeScan(
      device_path_,
      base::Bind(&base::DoNothing),
      base::Bind(&NetworkOperationErrorCallback, "ProposeScan"));
}

ChooseMobileNetworkHandler::~ChooseMobileNetworkHandler() {
  GetNetworkStateHandler()->RemoveObserver(this, FROM_HERE);
}

void ChooseMobileNetworkHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kJsApiCancel,
      base::Bind(&ChooseMobileNetworkHandler::HandleCancel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiConnect,
      base::Bind(&ChooseMobileNetworkHandler::HandleConnect,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kJsApiPageReady,
      base::Bind(&ChooseMobileNetworkHandler::HandlePageReady,
                 base::Unretained(this)));
}

void ChooseMobileNetworkHandler::DeviceListChanged() {
  const DeviceState* cellular = GetNetworkStateHandler()->GetDeviceState(
      device_path_);
  networks_list_.Clear();
  if (!cellular) {
    LOG(WARNING) << "Cellular device with path '" << device_path_
                 << "' disappeared.";
    return;
  }
  if (cellular->scanning()) {
    NET_LOG_EVENT("ChooseMobileNetwork", "Device is scanning for networks.");
    scanning_ = true;
    if (is_page_ready_)
      web_ui()->CallJavascriptFunctionUnsafe(kJsApiShowScanning);
    return;
  }
  scanning_ = false;
  const DeviceState::CellularScanResults& scan_results =
      cellular->scan_results();
  std::set<std::string> network_ids;
  for (DeviceState::CellularScanResults::const_iterator it =
      scan_results.begin(); it != scan_results.end(); ++it) {
    // We need to remove duplicates from the list because same network with
    // different technologies are listed multiple times. But ModemManager
    // Register API doesn't allow technology to be specified so just show unique
    // network in UI.
    if (network_ids.insert(it->network_id).second) {
      auto network = base::MakeUnique<base::DictionaryValue>();
      network->SetString(kNetworkIdProperty, it->network_id);
      if (!it->long_name.empty())
        network->SetString(kOperatorNameProperty, it->long_name);
      else if (!it->short_name.empty())
        network->SetString(kOperatorNameProperty, it->short_name);
      else
        network->SetString(kOperatorNameProperty, it->network_id);
      network->SetString(kStatusProperty, it->status);
      network->SetString(kTechnologyProperty, it->technology);
      networks_list_.Append(std::move(network));
    }
  }
  if (is_page_ready_) {
    web_ui()->CallJavascriptFunctionUnsafe(kJsApiShowNetworks, networks_list_);
    networks_list_.Clear();
    has_pending_results_ = false;
  } else {
    has_pending_results_ = true;
  }
}

void ChooseMobileNetworkHandler::HandleCancel(const base::ListValue* args) {
  const size_t kConnectParamCount = 0;
  if (args->GetSize() != kConnectParamCount) {
    NOTREACHED();
    return;
  }

  // Switch to automatic mode.
  GetNetworkDeviceHandler()->RegisterCellularNetwork(
      device_path_,
      "",  // An empty string is for registration with the home network.
      base::Bind(&base::DoNothing),
      base::Bind(&NetworkOperationErrorCallback,
                 "Register in automatic mode."));
}

void ChooseMobileNetworkHandler::HandleConnect(const base::ListValue* args) {
  std::string network_id;
  const size_t kConnectParamCount = 1;
  if (args->GetSize() != kConnectParamCount ||
      !args->GetString(0, &network_id)) {
    NOTREACHED();
    return;
  }

  GetNetworkDeviceHandler()->RegisterCellularNetwork(
      device_path_,
      network_id,
      base::Bind(&base::DoNothing),
      base::Bind(&NetworkOperationErrorCallback,
                 std::string("Register to network: ") + network_id));
}

void ChooseMobileNetworkHandler::HandlePageReady(const base::ListValue* args) {
  const size_t kConnectParamCount = 0;
  if (args->GetSize() != kConnectParamCount) {
    NOTREACHED();
    return;
  }

  if (has_pending_results_) {
    web_ui()->CallJavascriptFunctionUnsafe(kJsApiShowNetworks, networks_list_);
    networks_list_.Clear();
    has_pending_results_ = false;
  } else if (scanning_) {
    web_ui()->CallJavascriptFunctionUnsafe(kJsApiShowScanning);
  }
  is_page_ready_ = true;
}

}  // namespace

ChooseMobileNetworkUI::ChooseMobileNetworkUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<ChooseMobileNetworkHandler>());
  // Set up the "chrome://choose-mobile-network" source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(
      profile, CreateChooseMobileNetworkUIHTMLSource());
}

}  // namespace chromeos
