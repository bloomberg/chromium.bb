// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/choose_mobile_network_ui.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

namespace {

// JS API callbacks names.
const char kJsApiCancel[] = "cancel";
const char kJsApiConnect[] = "connect";

// Page JS API function names.
const char kJsApiShowNetworks[] = "mobile.ChooseNetwork.showNetworks";

// Network properties.
const char kNetworkIdProperty[] = "networkId";
const char kOperatorNameProperty[] = "operatorName";
const char kStatusProperty[] = "status";
const char kTechnologyProperty[] = "technology";

class ChooseMobileNetworkHTMLSource
    : public ChromeURLDataManager::DataSource {
 public:
  ChooseMobileNetworkHTMLSource();

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                bool is_incognito,
                                int request_id);
  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  virtual ~ChooseMobileNetworkHTMLSource() {}

  std::string service_path_;
  DISALLOW_COPY_AND_ASSIGN(ChooseMobileNetworkHTMLSource);
};

class ChooseMobileNetworkHandler
    : public WebUIMessageHandler,
      public NetworkLibrary::NetworkDeviceObserver {
 public:
  ChooseMobileNetworkHandler();
  virtual ~ChooseMobileNetworkHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // NetworkDeviceObserver implementation.
  virtual void OnNetworkDeviceChanged(NetworkLibrary* cros,
                                      const NetworkDevice* device);

 private:
  // Handlers for JS WebUI messages.
  void HandleCancel(const ListValue* args);
  void HandleConnect(const ListValue* args);

  std::string device_path_;

  DISALLOW_COPY_AND_ASSIGN(ChooseMobileNetworkHandler);
};

// ChooseMobileNetworkHTMLSource implementation.

ChooseMobileNetworkHTMLSource::ChooseMobileNetworkHTMLSource()
    : DataSource(chrome::kChromeUIChooseMobileNetworkHost,
                 MessageLoop::current()) {
}

void ChooseMobileNetworkHTMLSource::StartDataRequest(const std::string& path,
                                                       bool is_incognito,
                                                       int request_id) {
  DictionaryValue strings;
  strings.SetString(
      "chooseNetworkTitle",
      l10n_util::GetStringUTF16(IDS_NETWORK_CHOOSE_MOBILE_NETWORK));
  strings.SetString(
      "scanningMsgLine1",
      l10n_util::GetStringUTF16(IDS_NETWORK_SCANNING_FOR_MOBILE_NETWORKS));
  strings.SetString(
      "scanningMsgLine2",
      l10n_util::GetStringUTF16(IDS_NETWORK_SCANNING_THIS_MAY_TAKE_A_MINUTE));
  strings.SetString(
      "noMobileNetworks",
      l10n_util::GetStringUTF16(IDS_NETWORK_NO_MOBILE_NETWORKS));
  strings.SetString("connect",
                    l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_CONNECT));
  strings.SetString("cancel", l10n_util::GetStringUTF16(IDS_CANCEL));
  SetFontAndTextDirection(&strings);

  static const base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_CHOOSE_MOBILE_NETWORK_HTML));

  const std::string& full_html = jstemplate_builder::GetI18nTemplateHtml(
      html, &strings);

  scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes());
  html_bytes->data.resize(full_html.size());
  std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

  SendResponse(request_id, html_bytes);
}

// ChooseMobileNetworkHandler implementation.

ChooseMobileNetworkHandler::ChooseMobileNetworkHandler() {
  if (!CrosLibrary::Get()->EnsureLoaded())
    return;

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->RequestCellularScan();
  if (const NetworkDevice* cellular = cros->FindCellularDevice()) {
    device_path_ = cellular->device_path();
    cros->AddNetworkDeviceObserver(device_path_, this);
  }
}

ChooseMobileNetworkHandler::~ChooseMobileNetworkHandler() {
  if (!device_path_.empty()) {
    NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
    cros->RemoveNetworkDeviceObserver(device_path_, this);
  }
}

void ChooseMobileNetworkHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      kJsApiCancel,
      NewCallback(this, &ChooseMobileNetworkHandler::HandleCancel));
  web_ui_->RegisterMessageCallback(
      kJsApiConnect,
      NewCallback(this, &ChooseMobileNetworkHandler::HandleConnect));
}

void ChooseMobileNetworkHandler::OnNetworkDeviceChanged(
    NetworkLibrary* cros,
    const NetworkDevice* device) {

  ListValue networks_list;
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
      networks_list.Append(network);
    }
  }
  web_ui_->CallJavascriptFunction(kJsApiShowNetworks, networks_list);
}

void ChooseMobileNetworkHandler::HandleCancel(const ListValue* args) {
  const size_t kConnectParamCount = 0;
  if (args->GetSize() != kConnectParamCount) {
    NOTREACHED();
    return;
  }

  if (!CrosLibrary::Get()->EnsureLoaded())
    return;

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

  if (!CrosLibrary::Get()->EnsureLoaded())
    return;

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();
  cros->RequestCellularRegister(network_id);
}

}  // namespace

ChooseMobileNetworkUI::ChooseMobileNetworkUI(TabContents* contents)
    : WebUI(contents) {
  ChooseMobileNetworkHandler* handler = new ChooseMobileNetworkHandler();
  AddMessageHandler((handler)->Attach(this));
  ChooseMobileNetworkHTMLSource* html_source =
      new ChooseMobileNetworkHTMLSource();
  // Set up the "chrome://choose-mobile-network" source.
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}

}  // namespace chromeos
