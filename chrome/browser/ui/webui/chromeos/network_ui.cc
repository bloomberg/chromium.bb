// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_ui.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace chromeos {

namespace {

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network)
    return false;
  *service_path = network->path();
  return true;
}

void SetDeviceProperties(base::DictionaryValue* dictionary) {
  std::string device;
  dictionary->GetStringWithoutPathExpansion(shill::kDeviceProperty, &device);
  const DeviceState* device_state =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(device);
  if (!device_state)
    return;

  scoped_ptr<base::DictionaryValue> device_dictionary(
      device_state->properties().DeepCopy());

  if (!device_state->ip_configs().empty()) {
    // Convert IPConfig dictionary to a ListValue.
    scoped_ptr<base::ListValue> ip_configs(new base::ListValue);
    for (base::DictionaryValue::Iterator iter(device_state->ip_configs());
         !iter.IsAtEnd(); iter.Advance()) {
      ip_configs->Append(iter.value().DeepCopy());
    }
    device_dictionary->SetWithoutPathExpansion(shill::kIPConfigsProperty,
                                               ip_configs.release());
  }
  if (!device_dictionary->empty())
    dictionary->Set(shill::kDeviceProperty, device_dictionary.release());
}

class NetworkConfigMessageHandler : public content::WebUIMessageHandler {
 public:
  NetworkConfigMessageHandler() : weak_ptr_factory_(this) {}
  ~NetworkConfigMessageHandler() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getShillProperties",
        base::Bind(&NetworkConfigMessageHandler::GetShillProperties,
                   base::Unretained(this)));
  }

 private:
  void GetShillProperties(const base::ListValue* arg_list) {
    std::string guid;
    if (!arg_list->GetString(0, &guid)) {
      NOTREACHED();
      ErrorCallback(guid, "Missing GUID in arg list", nullptr);
      return;
    }
    std::string service_path;
    if (!GetServicePathFromGuid(guid, &service_path)) {
      ErrorCallback(guid, "Error.InvalidNetworkGuid", nullptr);
      return;
    }
    NetworkHandler::Get()->network_configuration_handler()->GetShillProperties(
        service_path,
        base::Bind(&NetworkConfigMessageHandler::GetShillPropertiesSuccess,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&NetworkConfigMessageHandler::ErrorCallback,
                   weak_ptr_factory_.GetWeakPtr(), guid));
  }

  void GetShillPropertiesSuccess(
      const std::string& service_path,
      const base::DictionaryValue& dictionary) const {
    scoped_ptr<base::DictionaryValue> dictionary_copy(dictionary.DeepCopy());

    // Set the 'ServicePath' property for debugging.
    dictionary_copy->SetStringWithoutPathExpansion("ServicePath", service_path);
    // Set the device properties for debugging.
    SetDeviceProperties(dictionary_copy.get());

    base::ListValue return_arg_list;
    return_arg_list.Append(dictionary_copy.release());
    web_ui()->CallJavascriptFunction("NetworkUI.getShillPropertiesResult",
                                     return_arg_list);
  }

  void ErrorCallback(
      const std::string& guid,
      const std::string& error_name,
      scoped_ptr<base::DictionaryValue> /* error_data */) const {
    NET_LOG(ERROR) << "Shill Error: " << error_name << " guid=" << guid;
    base::ListValue return_arg_list;
    scoped_ptr<base::DictionaryValue> dictionary;
    dictionary->SetStringWithoutPathExpansion(shill::kGuidProperty, guid);
    dictionary->SetStringWithoutPathExpansion("ShillError", error_name);
    return_arg_list.Append(dictionary.release());
    web_ui()->CallJavascriptFunction("NetworkUI.getShillPropertiesResult",
                                     return_arg_list);
  }

  base::WeakPtrFactory<NetworkConfigMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigMessageHandler);
};

}  // namespace

NetworkUI::NetworkUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(new NetworkConfigMessageHandler());

  // Enable extension API calls in the WebUI.
  extensions::TabHelper::CreateForWebContents(web_ui->GetWebContents());

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUINetworkHost);

  html->AddLocalizedString("titleText", IDS_NETWORK_UI_TITLE);
  html->AddLocalizedString("autoRefreshText", IDS_NETWORK_UI_AUTO_REFRESH);
  html->AddLocalizedString("deviceLogLinkText", IDS_DEVICE_LOG_LINK_TEXT);
  html->AddLocalizedString("networkRefreshText", IDS_NETWORK_UI_REFRESH);
  html->AddLocalizedString("clickToExpandText", IDS_NETWORK_UI_EXPAND);
  html->AddLocalizedString("propertyFormatText",
                           IDS_NETWORK_UI_PROPERTY_FORMAT);

  html->AddLocalizedString("normalFormatOption", IDS_NETWORK_UI_FORMAT_NORMAL);
  html->AddLocalizedString("managedFormatOption",
                           IDS_NETWORK_UI_FORMAT_MANAGED);
  html->AddLocalizedString("stateFormatOption", IDS_NETWORK_UI_FORMAT_STATE);
  html->AddLocalizedString("shillFormatOption", IDS_NETWORK_UI_FORMAT_SHILL);

  html->AddLocalizedString("visibleNetworksLabel",
                           IDS_NETWORK_UI_VISIBLE_NETWORKS);
  html->AddLocalizedString("favoriteNetworksLabel",
                           IDS_NETWORK_UI_FAVORITE_NETWORKS);

  html->AddLocalizedString("networkConnected",
                           IDS_ASH_STATUS_TRAY_NETWORK_CONNECTED);
  html->AddLocalizedString("networkConnecting",
                           IDS_ASH_STATUS_TRAY_NETWORK_CONNECTING);
  html->AddLocalizedString("networkDisabled",
                           IDS_OPTIONS_SETTINGS_NETWORK_DISABLED);
  html->AddLocalizedString("networkNotConnected",
                           IDS_ASH_STATUS_TRAY_NETWORK_NOT_CONNECTED);
  html->AddLocalizedString("OncTypeCellular", IDS_NETWORK_TYPE_CELLULAR);
  html->AddLocalizedString("OncTypeEthernet", IDS_NETWORK_TYPE_ETHERNET);
  html->AddLocalizedString("OncTypeVPN", IDS_NETWORK_TYPE_VPN);
  html->AddLocalizedString("OncTypeWiFi", IDS_NETWORK_TYPE_WIFI);
  html->AddLocalizedString("OncTypeWimax", IDS_NETWORK_TYPE_WIMAX);
  html->AddLocalizedString(
      "vpnNameTemplate",
      IDS_OPTIONS_SETTINGS_SECTION_THIRD_PARTY_VPN_NAME_TEMPLATE);

  html->SetJsonPath("strings.js");
  html->AddResourcePath("network_ui.css", IDR_NETWORK_UI_CSS);
  html->AddResourcePath("network_ui.js", IDR_NETWORK_UI_JS);
  html->SetDefaultResource(IDR_NETWORK_UI_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html);
}

NetworkUI::~NetworkUI() {
}

}  // namespace chromeos
