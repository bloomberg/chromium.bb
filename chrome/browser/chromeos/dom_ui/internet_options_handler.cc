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

  localized_strings->Set("wiredList", GetWiredList());
  localized_strings->Set("wirelessList", GetWirelessList());
  localized_strings->Set("rememberedList", GetRememberedList());
}

void InternetOptionsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  DCHECK(dom_ui_);
  dom_ui_->RegisterMessageCallback("buttonClickCallback",
      NewCallback(this, &InternetOptionsHandler::ButtonClickCallback));
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

void InternetOptionsHandler::CreateModalPopup(views::WindowDelegate* view) {
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser = BrowserList::FindBrowserWithProfile(dom_ui_->GetProfile());
  views::Window* window = views::Window::CreateChromeWindow(
      browser->window()->GetNativeHandle(), gfx::Rect(), view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
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
    CreateModalPopup(new chromeos::NetworkConfigView(cros->ethernet_network()));
  } else if (type == chromeos::TYPE_WIFI) {
    chromeos::WifiNetwork network;
    if (command == "forget") {
      cros->ForgetWirelessNetwork(service_path);
    } else if (service_path == kOtherNetworksFakePath) {
      // Other wifi networks.
      CreateModalPopup(new chromeos::NetworkConfigView());
    } else if (cros->FindWifiNetworkByPath(service_path, &network)) {
      if (command == "connect") {
        // Connect to wifi here. Open password page if appropriate.
        if (network.encrypted()) {
          chromeos::NetworkConfigView* view =
              new chromeos::NetworkConfigView(network, true);
          CreateModalPopup(view);
          view->SetLoginTextfieldFocus();
        } else {
          cros->ConnectToWifiNetwork(
              network, std::string(), std::string(), std::string());
        }
      } else if (command == "disconnect") {
        cros->DisconnectFromWirelessNetwork(network);
      } else if (command == "options") {
        CreateModalPopup(new chromeos::NetworkConfigView(network, false));
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
        CreateModalPopup(new chromeos::NetworkConfigView(network));
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
