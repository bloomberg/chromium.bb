// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/internet_options_handler.h"

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/base64.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/notification_service.h"
#include "gfx/codec/png_codec.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/window/window.h"

static const char kOtherNetworksFakePath[] = "?";

InternetOptionsHandler::InternetOptionsHandler() {
  chromeos::CrosLibrary::Get()->GetNetworkLibrary()->AddObserver(this);
}

InternetOptionsHandler::~InternetOptionsHandler() {
  chromeos::CrosLibrary::Get()->GetNetworkLibrary()->RemoveObserver(this);
}

void InternetOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Internet page - ChromeOS
  localized_strings->SetString("internetPage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_INTERNET_TAB_LABEL));

  localized_strings->SetString("wired_title",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_WIRED_NETWORK));
  localized_strings->SetString("wireless_title",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_WIRELESS_NETWORK));
  localized_strings->SetString("remembered_title",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_SECTION_TITLE_REMEMBERED_NETWORK));

  localized_strings->SetString("connect_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_CONNECT));
  localized_strings->SetString("disconnect_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_DISCONNECT));
  localized_strings->SetString("options_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_OPTIONS));
  localized_strings->SetString("forget_button",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_FORGET));

  localized_strings->SetString("inetAddress",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_ADDRESS));
  localized_strings->SetString("inetSubnetAddress",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SUBNETMASK));
  localized_strings->SetString("inetGateway",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_GATEWAY));
  localized_strings->SetString("inetDns",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DNSSERVER));

  localized_strings->SetString("inetSsid",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID));
  localized_strings->SetString("inetIdent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_IDENTITY));
  localized_strings->SetString("inetCert",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT));
  localized_strings->SetString("inetCertPass",
      l10n_util::GetStringUTF16(
           IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PRIVATE_KEY_PASSWORD));
  localized_strings->SetString("inetPass",
      l10n_util::GetStringUTF16(
           IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSPHRASE));
  localized_strings->SetString("inetRememberNetwork",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT));
  localized_strings->SetString("inetCertPkcs",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CERT_INSTALLED));
  localized_strings->SetString("inetLogin",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_LOGIN));
  localized_strings->SetString("inetShowPass",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SHOWPASSWORD));
  localized_strings->SetString("inetPassPrompt",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSWORD));
  localized_strings->SetString("inetSsidPrompt",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SSID));
  localized_strings->SetString("inetStatus",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_STATUS_TITLE));
  localized_strings->SetString("inetConnect",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CONNECT_TITLE));


  localized_strings->SetString("detailsInternetDismiss",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DONE));

  localized_strings->Set("wiredList", GetWiredList());
  localized_strings->Set("wirelessList", GetWirelessList());
  localized_strings->Set("rememberedList", GetRememberedList());
}

void InternetOptionsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback("buttonClickCallback",
      NewCallback(this, &InternetOptionsHandler::ButtonClickCallback));
  dom_ui_->RegisterMessageCallback("loginToNetwork",
      NewCallback(this, &InternetOptionsHandler::LoginCallback));
  dom_ui_->RegisterMessageCallback("loginToCertNetwork",
      NewCallback(this, &InternetOptionsHandler::LoginCertCallback));
  dom_ui_->RegisterMessageCallback("setDetails",
      NewCallback(this, &InternetOptionsHandler::SetDetailsCallback));
  dom_ui_->RegisterMessageCallback("loginToOtherNetwork",
      NewCallback(this, &InternetOptionsHandler::LoginToOtherCallback));

}

void InternetOptionsHandler::NetworkChanged(chromeos::NetworkLibrary* cros) {
  if (dom_ui_) {
    DictionaryValue dictionary;
    dictionary.Set("wiredList", GetWiredList());
    dictionary.Set("wirelessList", GetWirelessList());
    dictionary.Set("rememberedList", GetRememberedList());
    dom_ui_->CallJavascriptFunction(
        L"options.InternetOptions.refreshNetworkData", dictionary);
  }
}

void InternetOptionsHandler::SetDetailsCallback(const ListValue* args) {

  std::string service_path;

  if (!args->GetString(0, &service_path)) {
    NOTREACHED();
    return;
  }
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  chromeos::WifiNetwork network;

  if (cros->FindWifiNetworkByPath(service_path, &network)) {
    bool changed = false;
    if (network.encrypted()) {
      if (network.encryption() == chromeos::SECURITY_8021X) {
        std::string certpath;
        std::string ident;
        std::string certpass;
        bool remember;

        if (args->GetSize() != 5 ||
            !args->GetBoolean(1, &remember) ||
            !args->GetString(2, &ident) ||
            !args->GetString(3, &certpath) ||
            !args->GetString(4, &certpass)) {
          NOTREACHED();
          return;
        }

        bool auto_connect = remember;
        if (auto_connect != network.auto_connect()) {
          network.set_auto_connect(auto_connect);
          changed = true;
        }
        if (ident != network.identity()) {
          network.set_identity(ident);
          changed = true;
        }
        if (certpass != network.passphrase()) {
          network.set_passphrase(certpass);
          changed = true;
        }
        if (certpath != network.cert_path()) {
          network.set_cert_path(certpath);
          changed = true;
        }
      } else {
        std::string password;
        std::string remember;

        if (args->GetSize() != 5 ||
            !args->GetString(1, &remember) ||
            !args->GetString(4, &password)) {
          NOTREACHED();
          return;
        }

        bool auto_connect = (remember == "true");
        if (auto_connect != network.auto_connect()) {
          network.set_auto_connect(auto_connect);
          changed = true;
        }
        if (password != network.passphrase()) {
          network.set_passphrase(password);
          changed = true;
        }
      }
    }
    if (changed) {
      chromeos::CrosLibrary::Get()->GetNetworkLibrary()->SaveWifiNetwork(
          network);
    }
  }
}

// Parse 'path' to determine if the certificate is stored in a pkcs#11 device.
// flimflam recognizes the string "SETTINGS:" to specify authentication
// parameters. 'key_id=' indicates that the certificate is stored in a pkcs#11
// device. See src/third_party/flimflam/files/doc/service-api.txt.
bool InternetOptionsHandler::is_certificate_in_pkcs11(const std::string& path) {
  static const std::string settings_string("SETTINGS:");
  static const std::string pkcs11_key("key_id");
  if (path.find(settings_string) == 0) {
    std::string::size_type idx = path.find(pkcs11_key);
    if (idx != std::string::npos)
      idx = path.find_first_not_of(kWhitespaceASCII, idx + pkcs11_key.length());
    if (idx != std::string::npos && path[idx] == '=')
      return true;
  }
  return false;
}

void InternetOptionsHandler::PopulateDictionaryDetails(
    const chromeos::Network& net, chromeos::NetworkLibrary* cros) {
  DictionaryValue dictionary;
  chromeos::ConnectionType type = net.type();
  chromeos::NetworkIPConfigVector ipconfigs =
      cros->GetIPConfigs(net.device_path());
  for (chromeos::NetworkIPConfigVector::const_iterator it = ipconfigs.begin();
       it != ipconfigs.end(); ++it) {
    const chromeos::NetworkIPConfig& ipconfig = *it;
    dictionary.SetString("address", ipconfig.address);
    dictionary.SetString("subnetAddress", ipconfig.netmask);
    dictionary.SetString("gateway", ipconfig.gateway);
    dictionary.SetString("dns", ipconfig.name_servers);
    dictionary.SetInteger("type", type);
    dictionary.SetString("servicePath", net.service_path());
    dictionary.SetBoolean("connecting", net.connecting());
    dictionary.SetBoolean("connected", net.connected());
    if (type == chromeos::TYPE_WIFI) {
      chromeos::WifiNetwork wireless;
      cros->FindWifiNetworkByPath(net.service_path(), &wireless);
      dictionary.SetString("ssid", wireless.name());
      dictionary.SetBoolean("autoConnect",wireless.auto_connect());
      if (wireless.encrypted()) {
        dictionary.SetBoolean("encrypted", true);
        if (wireless.encryption() == chromeos::SECURITY_8021X) {
          bool certificate_in_pkcs11 =
              is_certificate_in_pkcs11(wireless.cert_path());
          if (certificate_in_pkcs11) {
            dictionary.SetBoolean("certInPkcs", true);
          } else {
            dictionary.SetBoolean("certInPkcs", false);
          }
          dictionary.SetString("certPath",wireless.cert_path());
          dictionary.SetString("ident",wireless.identity());
          dictionary.SetBoolean("certNeeded", true);
          dictionary.SetString("certPass",wireless.passphrase());
        } else {
          dictionary.SetBoolean("certNeeded", false);
          dictionary.SetString("pass", wireless.passphrase());
        }
      } else {
        dictionary.SetBoolean("encrypted", false);
      }
    }
  }
  dom_ui_->CallJavascriptFunction(
      L"options.InternetOptions.showDetailedInfo", dictionary);
}

void InternetOptionsHandler::PopupWirelessPassword(
    const chromeos::WifiNetwork& network) {
  DictionaryValue dictionary;
  dictionary.SetString("servicePath",network.service_path());
  if (network.encryption() == chromeos::SECURITY_8021X) {
    dictionary.SetBoolean("certNeeded", true);
    dictionary.SetString("ident", network.identity());
    dictionary.SetString("cert", network.cert_path());
  } else {
    dictionary.SetBoolean("certNeeded", false);
    dictionary.SetString("pass", network.passphrase());
  }
  dom_ui_->CallJavascriptFunction(
      L"options.InternetOptions.showPasswordEntry", dictionary);
}

void InternetOptionsHandler::LoginCallback(const ListValue* args) {

  std::string service_path;
  std::string password;

  if (args->GetSize() != 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &password)) {
    NOTREACHED();
    return;
  }

  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  chromeos::WifiNetwork network;

  if (cros->FindWifiNetworkByPath(service_path, &network)) {
    cros->ConnectToWifiNetwork(
        network, password, std::string(), std::string());
  } else {
    // Must be an "other" login
    cros->ConnectToWifiNetwork(
        service_path, password, std::string(), std::string(), true);
  }
}

void InternetOptionsHandler::LoginCertCallback(const ListValue* args) {

  std::string service_path;
  std::string identity;
  std::string certpath;
  std::string password;

  if (args->GetSize() != 4 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &certpath) ||
      !args->GetString(2, &identity) ||
      !args->GetString(3, &password)) {
    NOTREACHED();
    return;
  }
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  chromeos::WifiNetwork network;

  if (cros->FindWifiNetworkByPath(service_path, &network)) {
    cros->ConnectToWifiNetwork(
        network, password, identity, certpath);
  } else {
    // TODO(dhg): Send error back to UI
  }
}

void InternetOptionsHandler::LoginToOtherCallback(const ListValue* args) {
  std::string ssid;
  std::string password;

  if (args->GetSize() != 2 ||
      !args->GetString(0, &ssid) ||
      !args->GetString(1, &password)) {
    NOTREACHED();
    return;
  }

  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();

  cros->ConnectToWifiNetwork(
      ssid, password, std::string(), std::string(), true);
}

void InternetOptionsHandler::ButtonClickCallback(const ListValue* args) {
  std::string str_type;
  std::string service_path;
  std::string command;
  if (args->GetSize() != 3 ||
      !args->GetString(0, &str_type) ||
      !args->GetString(1, &service_path) ||
      !args->GetString(2, &command)) {
    NOTREACHED();
    return;
  }

  int type = atoi(str_type.c_str());
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();

  if (type == chromeos::TYPE_ETHERNET) {
    const chromeos::EthernetNetwork& ether = cros->ethernet_network();
    PopulateDictionaryDetails(ether, cros);
  } else if (type == chromeos::TYPE_WIFI) {
    chromeos::WifiNetwork network;
    if (command == "forget") {
      cros->ForgetWirelessNetwork(service_path);
    } else if (cros->FindWifiNetworkByPath(service_path, &network)) {
      if (command == "connect") {
        // Connect to wifi here. Open password page if appropriate.
        if (network.encrypted()) {
          if (network.encryption() == chromeos::SECURITY_8021X) {
            PopulateDictionaryDetails(network, cros);
          } else {
            PopupWirelessPassword(network);
          }
        } else {
          cros->ConnectToWifiNetwork(
              network, std::string(), std::string(), std::string());
        }
      } else if (command == "disconnect") {
        cros->DisconnectFromWirelessNetwork(network);
      } else if (command == "options") {
        PopulateDictionaryDetails(network, cros);
      }
    }
  } else if (type == chromeos::TYPE_CELLULAR) {
    chromeos::CellularNetwork network;
    if (command == "forget") {
      cros->ForgetWirelessNetwork(service_path);
    } else if (cros->FindCellularNetworkByPath(service_path, &network)) {
      if (command == "connect") {
        cros->ConnectToCellularNetwork(network);
      } else if (command == "disconnect") {
        cros->DisconnectFromWirelessNetwork(network);
      } else if (command == "options") {
        PopulateDictionaryDetails(network, cros);
      }
    }
  } else {
    NOTREACHED();
  }
}

// Helper function to convert an icon to a data: URL
// with the icon encoded as a PNG.
static std::string ConvertSkBitmapToDataURL(const SkBitmap& icon) {
  DCHECK(!icon.isNull());

  // Get the icon data.
  std::vector<unsigned char> icon_data;
  gfx::PNGCodec::EncodeBGRASkBitmap(icon, false, &icon_data);

  // Base64-encode it (to make it a data URL).
  std::string icon_data_str(reinterpret_cast<char*>(&icon_data[0]),
                            icon_data.size());
  std::string icon_base64_encoded;
  base::Base64Encode(icon_data_str, &icon_base64_encoded);
  return std::string("data:image/png;base64," + icon_base64_encoded);
}

ListValue* InternetOptionsHandler::GetNetwork(const std::string& service_path,
    const SkBitmap& icon, const std::string& name, bool connecting,
    bool connected, int connection_type, bool remembered) {

  ListValue* network = new ListValue();

  int s = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
  if (connecting)
    s = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
  else if (connected)
    s = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
  string16 status = l10n_util::GetStringUTF16(s);

  // service path
  network->Append(Value::CreateStringValue(service_path));
  // name
  network->Append(Value::CreateStringValue(name));
  // status
  network->Append(Value::CreateStringValue(status));
  // type
  network->Append(Value::CreateIntegerValue(connection_type));
  // connected
  network->Append(Value::CreateBooleanValue(connected));
  // connecting
  network->Append(Value::CreateBooleanValue(connecting));
  // icon data url
  network->Append(Value::CreateStringValue(icon.isNull() ? "" :
      ConvertSkBitmapToDataURL(icon)));
  // remembered
  network->Append(Value::CreateBooleanValue(remembered));
  return network;
}

ListValue* InternetOptionsHandler::GetWiredList() {
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ListValue* list = new ListValue();

  // If ethernet is not enabled, then don't add anything.
  if (cros->ethernet_enabled()) {
    const chromeos::EthernetNetwork& ethernet_network =
        cros->ethernet_network();
    SkBitmap icon = *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED_BLACK);
    if (!ethernet_network.connecting() &&
        !ethernet_network.connected()) {
      icon = chromeos::NetworkMenu::IconForDisplay(icon,
          *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED));
    }
    list->Append(GetNetwork(
        ethernet_network.service_path(),
        icon,
        l10n_util::GetStringUTF8(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET),
        ethernet_network.connecting(),
        ethernet_network.connected(),
        chromeos::TYPE_ETHERNET,
        false));
  }
  return list;
}

ListValue* InternetOptionsHandler::GetWirelessList() {
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ListValue* list = new ListValue();

  const chromeos::WifiNetworkVector& wifi_networks = cros->wifi_networks();
  for (chromeos::WifiNetworkVector::const_iterator it =
      wifi_networks.begin(); it != wifi_networks.end(); ++it) {
    SkBitmap icon = chromeos::NetworkMenu::IconForNetworkStrength(
        it->strength(), true);
    if (it->encrypted()) {
      icon = chromeos::NetworkMenu::IconForDisplay(icon,
          *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE));
    }
    list->Append(GetNetwork(
        it->service_path(),
        icon,
        it->name(),
        it->connecting(),
        it->connected(),
        chromeos::TYPE_WIFI,
        false));
  }

  const chromeos::CellularNetworkVector& cellular_networks =
      cros->cellular_networks();
  for (chromeos::CellularNetworkVector::const_iterator it =
      cellular_networks.begin(); it != cellular_networks.end(); ++it) {
    SkBitmap icon = chromeos::NetworkMenu::IconForNetworkStrength(
        it->strength(), true);
    SkBitmap badge = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G);
    icon = chromeos::NetworkMenu::IconForDisplay(icon, badge);
    list->Append(GetNetwork(
        it->service_path(),
        icon,
        it->name(),
        it->connecting(),
        it->connected(),
        chromeos::TYPE_CELLULAR,
        false));
  }

  // Add "Other..." if wifi is enabled.
  if (cros->wifi_enabled()) {
    list->Append(GetNetwork(
        kOtherNetworksFakePath,
        SkBitmap(),
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_OTHER_NETWORKS),
        false,
        false,
        chromeos::TYPE_WIFI,
        false));
  }

  return list;
}

ListValue* InternetOptionsHandler::GetRememberedList() {
  chromeos::NetworkLibrary* cros =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  ListValue* list = new ListValue();

  const chromeos::WifiNetworkVector& wifi_networks =
      cros->remembered_wifi_networks();
  for (chromeos::WifiNetworkVector::const_iterator it =
      wifi_networks.begin(); it != wifi_networks.end(); ++it) {
    SkBitmap icon = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0);
    if (it->encrypted()) {
      icon = chromeos::NetworkMenu::IconForDisplay(icon,
          *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_SECURE));
    }
    list->Append(GetNetwork(
        it->service_path(),
        icon,
        it->name(),
        it->connecting(),
        it->connected(),
        chromeos::TYPE_WIFI,
        true));
  }

  const chromeos::CellularNetworkVector& cellular_networks =
      cros->remembered_cellular_networks();
  for (chromeos::CellularNetworkVector::const_iterator it =
      cellular_networks.begin(); it != cellular_networks.end(); ++it) {
    SkBitmap icon = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0);
    SkBitmap badge = *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G);
    icon = chromeos::NetworkMenu::IconForDisplay(icon, badge);
    list->Append(GetNetwork(
        it->service_path(),
        icon,
        it->name(),
        it->connecting(),
        it->connected(),
        chromeos::TYPE_CELLULAR,
        true));
  }

  return list;
}
