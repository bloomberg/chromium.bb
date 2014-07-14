// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/network_dropdown.h"

#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "chromeos/network/network_state_handler.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/network/network_icon.h"
#include "ui/chromeos/network/network_icon_animation.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// Timeout between consecutive requests to network library for network
// scan.
const int kNetworkScanIntervalSecs = 60;

}  // namespace

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
    gfx::Image icon;
    if (model->GetIconAt(i, &icon)) {
      SkBitmap icon_bitmap = icon.ToImageSkia()->GetRepresentation(
          web_ui_->GetDeviceScaleFactor()).sk_bitmap();
      item->SetString("icon", webui::GetBitmapDataUrl(icon_bitmap));
    }
    if (id >= 0) {
      item->SetBoolean("enabled", model->IsEnabledAt(i));
      const gfx::FontList* font_list = model->GetLabelFontListAt(i);
      if (font_list)
        item->SetBoolean("bold", font_list->GetFontStyle() == gfx::Font::BOLD);
    }
    if (type == ui::MenuModel::TYPE_SUBMENU)
      item->Set("sub", ConvertMenuModel(model->GetSubmenuModelAt(i)));
    list->Append(item);
  }
  return list;
}

// NetworkDropdown -------------------------------------------------------------

NetworkDropdown::NetworkDropdown(Actor* actor,
                                 content::WebUI* web_ui,
                                 bool oobe)
    : actor_(actor),
      web_ui_(web_ui),
      oobe_(oobe) {
  DCHECK(actor_);
  network_menu_.reset(new NetworkMenuWebUI(this, web_ui));
  DCHECK(NetworkHandler::IsInitialized());
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  handler->RequestScan();
  handler->AddObserver(this, FROM_HERE);
  Refresh();
  network_scan_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(kNetworkScanIntervalSecs),
      this, &NetworkDropdown::RequestNetworkScan);
}

NetworkDropdown::~NetworkDropdown() {
  ui::network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
}

void NetworkDropdown::OnItemChosen(int id) {
  network_menu_->OnItemChosen(id);
}

gfx::NativeWindow NetworkDropdown::GetNativeWindow() const {
  return LoginDisplayHostImpl::default_host()->GetNativeWindow();
}

void NetworkDropdown::OpenButtonOptions() {
  LoginDisplayHostImpl::default_host()->OpenProxySettings();
}

bool NetworkDropdown::ShouldOpenButtonOptions() const {
  return !oobe_;
}

void NetworkDropdown::OnConnectToNetworkRequested() {
  actor_->OnConnectToNetworkRequested();
}

void NetworkDropdown::DefaultNetworkChanged(const NetworkState* network) {
  Refresh();
}

void NetworkDropdown::NetworkConnectionStateChanged(
    const NetworkState* network) {
  Refresh();
}

void NetworkDropdown::NetworkListChanged() {
  Refresh();
}

void NetworkDropdown::NetworkIconChanged() {
  SetNetworkIconAndText();
}

void NetworkDropdown::Refresh() {
  SetNetworkIconAndText();
  network_menu_->UpdateMenu();
}

void NetworkDropdown::SetNetworkIconAndText() {
  base::string16 text;
  gfx::ImageSkia icon_image;
  bool animating = false;
  ui::network_icon::GetDefaultNetworkImageAndLabel(
      ui::network_icon::ICON_TYPE_LIST, &icon_image, &text, &animating);
  if (animating) {
    ui::network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  } else {
    ui::network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }
  SkBitmap icon_bitmap = icon_image.GetRepresentation(
      web_ui_->GetDeviceScaleFactor()).sk_bitmap();
  std::string icon_str;
  if (!icon_image.isNull())
    icon_str = webui::GetBitmapDataUrl(icon_bitmap);
  base::StringValue title(text);
  base::StringValue icon(icon_str);
  web_ui_->CallJavascriptFunction("cr.ui.DropDown.updateNetworkTitle",
                                  title, icon);
}

void NetworkDropdown::RequestNetworkScan() {
  NetworkHandler::Get()->network_state_handler()->RequestScan();
  Refresh();
}

}  // namespace chromeos
