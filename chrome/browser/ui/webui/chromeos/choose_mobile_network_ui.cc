// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/choose_mobile_network_ui.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

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

// Network properties.
const char kNetworkIdProperty[] = "networkId";
const char kOperatorNameProperty[] = "operatorName";
const char kStatusProperty[] = "status";
const char kTechnologyProperty[] = "technology";

ChromeWebUIDataSource* CreateChooseMobileNetworkUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIChooseMobileNetworkHost);

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

  source->set_json_path("strings.js");
  source->add_resource_path("choose_mobile_network.js",
                            IDR_CHOOSE_MOBILE_NETWORK_JS);
  source->set_default_resource(IDR_CHOOSE_MOBILE_NETWORK_HTML);
  return source;
}

class ChooseMobileNetworkHandler
    : public WebUIMessageHandler,
      public NetworkLibrary::NetworkDeviceObserver {
 public:
  ChooseMobileNetworkHandler();
  virtual ~ChooseMobileNetworkHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // NetworkDeviceObserver implementation.
  virtual void OnNetworkDeviceFoundNetworks(
      NetworkLibrary* cros, const NetworkDevice* device) OVERRIDE;

 private:
  // Handlers for JS WebUI messages.
  void HandleCancel(const ListValue* args);
  void HandleConnect(const ListValue* args);
  void HandlePageReady(const ListValue* args);

  std::string device_path_;
  ListValue networks_list_;
  bool is_page_ready_;
  bool has_pending_results_;

  DISALLOW_COPY_AND_ASSIGN(ChooseMobileNetworkHandler);
};

// ChooseMobileNetworkHandler implementation.

ChooseMobileNetworkHandler::ChooseMobileNetworkHandler()
    : is_page_ready_(false), has_pending_results_(false) {
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  if (const NetworkDevice* cellular = cros->FindCellularDevice()) {
    device_path_ = cellular->device_path();
    cros->AddNetworkDeviceObserver(device_path_, this);
  }
  cros->RequestCellularScan();
}

ChooseMobileNetworkHandler::~ChooseMobileNetworkHandler() {
  if (!device_path_.empty()) {
    NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
    cros->RemoveNetworkDeviceObserver(device_path_, this);
  }
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

void ChooseMobileNetworkHandler::OnNetworkDeviceFoundNetworks(
    NetworkLibrary* cros,
    const NetworkDevice* device) {
  networks_list_.Clear();
  std::set<std::string> network_ids;
  const CellularNetworkList& found_networks = device->found_cellular_networks();
  for (CellularNetworkList::const_iterator it = found_networks.begin();
       it != found_networks.end(); ++it) {
    // We need to remove duplicates from the list because same network with
    // different technologies are listed multiple times. But ModemManager
    // Register API doesn't allow technology to be specified so just show unique
    // network in UI.
    if (network_ids.insert(it->network_id).second) {
      DictionaryValue* network = new DictionaryValue();
      network->SetString(kNetworkIdProperty, it->network_id);
      if (!it->long_name.empty())
        network->SetString(kOperatorNameProperty, it->long_name);
      else if (!it->short_name.empty())
        network->SetString(kOperatorNameProperty, it->short_name);
      else
        network->SetString(kOperatorNameProperty, it->network_id);
      network->SetString(kStatusProperty, it->status);
      network->SetString(kTechnologyProperty, it->technology);
      networks_list_.Append(network);
    }
  }
  if (is_page_ready_) {
    web_ui()->CallJavascriptFunction(kJsApiShowNetworks, networks_list_);
    networks_list_.Clear();
    has_pending_results_ = false;
  } else {
    has_pending_results_ = true;
  }
}

void ChooseMobileNetworkHandler::HandleCancel(const ListValue* args) {
  const size_t kConnectParamCount = 0;
  if (args->GetSize() != kConnectParamCount) {
    NOTREACHED();
    return;
  }

  // Switch to automatic mode.
  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->RequestCellularRegister(std::string());
}

void ChooseMobileNetworkHandler::HandleConnect(const ListValue* args) {
  std::string network_id;
  const size_t kConnectParamCount = 1;
  if (args->GetSize() != kConnectParamCount ||
      !args->GetString(0, &network_id)) {
    NOTREACHED();
    return;
  }

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->RequestCellularRegister(network_id);
}

void ChooseMobileNetworkHandler::HandlePageReady(const ListValue* args) {
  const size_t kConnectParamCount = 0;
  if (args->GetSize() != kConnectParamCount) {
    NOTREACHED();
    return;
  }

  if (has_pending_results_) {
    web_ui()->CallJavascriptFunction(kJsApiShowNetworks, networks_list_);
    networks_list_.Clear();
    has_pending_results_ = false;
  }
  is_page_ready_ = true;
}

}  // namespace

ChooseMobileNetworkUI::ChooseMobileNetworkUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  ChooseMobileNetworkHandler* handler = new ChooseMobileNetworkHandler();
  web_ui->AddMessageHandler(handler);
  // Set up the "chrome://choose-mobile-network" source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(
      profile, CreateChooseMobileNetworkUIHTMLSource());
}

}  // namespace chromeos
