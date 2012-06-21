// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_dropdown.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/base_login_display_host.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/font.h"

namespace chromeos {

// WebUI specific implementation of the NetworkMenu class.
class NetworkMenuWebUI : public NetworkMenu {
 public:
  NetworkMenuWebUI(NetworkMenu::Delegate* delegate, content::WebUI* web_ui);

  // NetworkMenu override:
  virtual void UpdateMenu() OVERRIDE;

  // Called when item with command |id| is chosen.
  void OnItemChosen(int id);

 private:
  // Converts menu model into the ListValue, ready for passing to WebUI.
  base::ListValue* ConvertMenuModel(ui::MenuModel* model);

  // WebUI where network menu is located.
  content::WebUI* web_ui_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMenuWebUI);
};

// NetworkMenuWebUI ------------------------------------------------------------

NetworkMenuWebUI::NetworkMenuWebUI(NetworkMenu::Delegate* delegate,
                                   content::WebUI* web_ui)
    : NetworkMenu(delegate),
      web_ui_(web_ui) {
}

void NetworkMenuWebUI::UpdateMenu() {
  NetworkMenu::UpdateMenu();
  if (web_ui_) {
    scoped_ptr<base::ListValue> list(ConvertMenuModel(GetMenuModel()));
    web_ui_->CallJavascriptFunction("cr.ui.DropDown.updateNetworks", *list);
  }
}

void NetworkMenuWebUI::OnItemChosen(int id) {
  int index;
  ui::MenuModel* model = GetMenuModel();
   if (!ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index))
     return;
  model->ActivatedAt(index);
}

base::ListValue* NetworkMenuWebUI::ConvertMenuModel(ui::MenuModel* model) {
  base::ListValue* list = new base::ListValue();
  for (int i = 0; i < model->GetItemCount(); ++i) {
    ui::MenuModel::ItemType type = model->GetTypeAt(i);
    int id;
    if (type == ui::MenuModel::TYPE_SEPARATOR)
      id = -2;
    else
      id = model->GetCommandIdAt(i);
    base::DictionaryValue* item = new base::DictionaryValue();
    item->SetInteger("id", id);
    item->SetString("label", model->GetLabelAt(i));
    gfx::ImageSkia icon;
    if (model->GetIconAt(i, &icon)) {
      float icon_scale;
      SkBitmap icon_bitmap = icon.GetBitmapForScale(
          web_ui_->GetDeviceScale(), &icon_scale);
      item->SetString("icon", web_ui_util::GetImageDataUrl(icon_bitmap));
    }
    if (id >= 0) {
      item->SetBoolean("enabled", model->IsEnabledAt(i));
      const gfx::Font* font = model->GetLabelFontAt(i);
      if (font) {
        item->SetBoolean("bold", font->GetStyle() == gfx::Font::BOLD);
      }
    }
    if (type == ui::MenuModel::TYPE_SUBMENU)
      item->Set("sub", ConvertMenuModel(model->GetSubmenuModelAt(i)));
    list->Append(item);
  }
  return list;
}

// NetworkDropdown -------------------------------------------------------------

NetworkDropdown::NetworkDropdown(content::WebUI* web_ui,
                                 bool oobe)
    : web_ui_(web_ui),
      oobe_(oobe) {
  network_menu_.reset(new NetworkMenuWebUI(this, web_ui));
  network_icon_.reset(
      new NetworkMenuIcon(this, NetworkMenuIcon::DROPDOWN_MODE));
  CrosLibrary::Get()->GetNetworkLibrary()->AddNetworkManagerObserver(this);
  CrosLibrary::Get()->GetNetworkLibrary()->RequestNetworkScan();
  Refresh();
}

NetworkDropdown::~NetworkDropdown() {
  CrosLibrary::Get()->GetNetworkLibrary()->RemoveNetworkManagerObserver(this);
}

void NetworkDropdown::SetLastNetworkType(ConnectionType last_network_type) {
  network_icon_->set_last_network_type(last_network_type);
}

void NetworkDropdown::OnItemChosen(int id) {
  network_menu_->OnItemChosen(id);
}

gfx::NativeWindow NetworkDropdown::GetNativeWindow() const {
  return BaseLoginDisplayHost::default_host()->GetNativeWindow();
}

void NetworkDropdown::OpenButtonOptions() {
  BaseLoginDisplayHost::default_host()->OpenProxySettings();
}

bool NetworkDropdown::ShouldOpenButtonOptions() const {
  return !oobe_;
}

void NetworkDropdown::OnNetworkManagerChanged(NetworkLibrary* cros) {
  Refresh();
}

void NetworkDropdown::Refresh() {
  SetNetworkIconAndText();
  network_menu_->UpdateMenu();
}

void NetworkDropdown::NetworkMenuIconChanged() {
  SetNetworkIconAndText();
}

void NetworkDropdown::SetNetworkIconAndText() {
  string16 text;
  const gfx::ImageSkia icon_image = network_icon_->GetIconAndText(&text);
  float icon_scale;
  SkBitmap icon_bitmap = icon_image.GetBitmapForScale(
      web_ui_->GetDeviceScale(), &icon_scale);
  std::string icon_str =
      icon_image.empty() ?
          std::string() : web_ui_util::GetImageDataUrl(icon_bitmap);
  base::StringValue title(text);
  base::StringValue icon(icon_str);
  web_ui_->CallJavascriptFunction("cr.ui.DropDown.updateNetworkTitle",
                                  title, icon);
}

}  // namespace chromeos
