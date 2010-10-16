// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/network_menu_ui.h"

#include "app/l10n_util.h"
#include "base/values.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/status/network_menu.h"
#include "chrome/browser/chromeos/views/domui_menu_widget.h"
#include "chrome/browser/chromeos/views/native_menu_domui.h"
#include "chrome/browser/dom_ui/dom_ui_theme_source.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "views/controls/menu/menu_2.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
//
// MenuHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages related to the "system" view.
class NetworkMenuHandler : public chromeos::MenuHandlerBase,
                           public base::SupportsWeakPtr<NetworkMenuHandler> {
 public:
  NetworkMenuHandler();
  virtual ~NetworkMenuHandler();

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  void HandleAction(const ListValue* values);

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuHandler);
};

void NetworkMenuHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "action",
      NewCallback(this,
                  &NetworkMenuHandler::HandleAction));
}

void NetworkMenuHandler::HandleAction(const ListValue* values) {
  menus::MenuModel* model = GetMenuModel();
  if (model) {
    chromeos::NetworkMenuUI* network_menu_ui =
        static_cast<chromeos::NetworkMenuUI*>(dom_ui_);
    bool close_menu = network_menu_ui->ModelAction(model, values);
    if (close_menu) {
      chromeos::DOMUIMenuWidget* widget
          = chromeos::DOMUIMenuWidget::FindDOMUIMenuWidget(
              dom_ui_->tab_contents()->GetNativeView());
      if (widget) {
        chromeos::NativeMenuDOMUI* domui_menu = widget->domui_menu();
        if (domui_menu)
          domui_menu->Hide();
      }
    }
  }
}

NetworkMenuHandler::NetworkMenuHandler() {
}

NetworkMenuHandler::~NetworkMenuHandler() {
}

} // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
//
// NetworkMenuUI
//
////////////////////////////////////////////////////////////////////////////////

NetworkMenuUI::NetworkMenuUI(TabContents* contents)
    : chromeos::MenuUI(
        contents,
        ALLOW_THIS_IN_INITIALIZER_LIST(
            CreateMenuUIHTMLSource(*this,
                                   chrome::kChromeUINetworkMenu,
                                   "NetworkMenu",
                                   IDR_NETWORK_MENU_JS,
                                   IDR_NETWORK_MENU_CSS))) {
  NetworkMenuHandler* handler = new NetworkMenuHandler();
  AddMessageHandler((handler)->Attach(this));

  // Set up chrome://theme/ source.
  DOMUIThemeSource* theme = new DOMUIThemeSource(GetProfile());
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(
          Singleton<ChromeURLDataManager>::get(),
          &ChromeURLDataManager::AddDataSource,
          make_scoped_refptr(theme)));
}

void NetworkMenuUI::AddCustomConfigValues(DictionaryValue* config) const {
}

void NetworkMenuUI::AddLocalizedStrings(
    DictionaryValue* localized_strings) const {
  DCHECK(localized_strings);

  localized_strings->SetString("reconnect", l10n_util::GetStringUTF16(
      IDS_NETWORK_RECONNECT_TITLE));
  localized_strings->SetString("remeber_this_network",
      l10n_util::GetStringUTF16(IDS_NETWORK_REMEMBER_THIS_NETWORK_TITLE));
  localized_strings->SetString("ssid_prompt",
      l10n_util::GetStringUTF16(IDS_NETWORK_SSID_HINT));
  localized_strings->SetString("pass_prompt",
      l10n_util::GetStringUTF16(IDS_NETWORK_PASSWORD_HINT));
}

bool NetworkMenuUI::ModelAction(const menus::MenuModel* model,
                                const ListValue* values) {
  const NetworkMenu* network_menu = static_cast<const NetworkMenu*>(model);
  std::string action;
  bool success = values->GetString(0, &action);
  bool close_menu = true;
  if (!success) {
    LOG(WARNING) << "ModelAction called with no arguments from: "
                 << chrome::kChromeUINetworkMenu;
    return close_menu;
  }
  int index;
  std::string index_str;
  success = values->GetString(1, &index_str);
  success = success && base::StringToInt(index_str, &index);
  if (!success) {
    LOG(WARNING) << "ModelAction called with no index from: "
                 << chrome::kChromeUINetworkMenu;
    return close_menu;
  }
  std::string passphrase;
  values->GetString(2, &passphrase);  // Optional
  std::string ssid;
  values->GetString(3, &ssid);  // Optional
  int remember = -1;  // -1 indicates not set
  std::string remember_str;
  if (values->GetString(4, &remember_str))  // Optional
    base::StringToInt(remember_str, &remember);

  if (action == "connect" || action == "reconnect") {
    close_menu = network_menu->ConnectToNetworkAt(index, passphrase, ssid,
                                                  remember);
  } else {
    LOG(WARNING) << "Unrecognized action: " << action
                 << " from: " << chrome::kChromeUINetworkMenu;
  }
  return close_menu;
}

DictionaryValue* NetworkMenuUI::CreateMenuItem(const menus::MenuModel* model,
                                               int index,
                                               const char* type,
                                               int* max_icon_width,
                                               bool* has_accel) const {
  // Create a MenuUI menu item, then append network specific values.
  DictionaryValue* item = MenuUI::CreateMenuItem(model,
                                                 index,
                                                 type,
                                                 max_icon_width,
                                                 has_accel);
  // Network menu specific values.
  const NetworkMenu* network_menu = static_cast<const NetworkMenu*>(model);
  NetworkMenu::NetworkInfo info;
  bool found = network_menu->GetNetworkAt(index, &info);

  item->SetBoolean("visible", found);
  item->SetString("network_type", info.network_type);
  item->SetString("status", info.status);
  item->SetString("message", info.message);
  item->SetString("ip_address", info.ip_address);
  item->SetBoolean("need_passphrase", info.need_passphrase);
  item->SetBoolean("remembered", info.remembered);
  return item;
}

views::Menu2* NetworkMenuUI::CreateMenu2(menus::MenuModel* model) {
  views::Menu2* menu = new views::Menu2(model);
  NativeMenuDOMUI::SetMenuURL(
      menu, GURL(StringPrintf("chrome://%s", chrome::kChromeUINetworkMenu)));
  return menu;
}

}  // namespace chromeos
