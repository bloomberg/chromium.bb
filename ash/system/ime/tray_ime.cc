// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/tray_ime.h"

#include <vector>

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_views.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace internal {

namespace tray {

class IMEDefaultView : public TrayItemMore {
 public:
  explicit IMEDefaultView(SystemTrayItem* owner)
      : TrayItemMore(owner) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    SetImage(bundle.GetImageNamed(
        IDR_AURA_UBER_TRAY_IME).ToSkBitmap());

    IMEInfo info;
    Shell::GetInstance()->tray_delegate()->GetCurrentIME(&info);
    UpdateLabel(info);
  }

  virtual ~IMEDefaultView() {}

  void UpdateLabel(const IMEInfo& info) {
    SetLabel(info.name);
    SetAccessibleName(info.name);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IMEDefaultView);
};

class IMEDetailedView : public views::View,
                        public ViewClickListener {
 public:
  IMEDetailedView(SystemTrayItem* owner, user::LoginStatus login)
      : login_(login),
        header_(NULL) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 1, 1, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    IMEInfoList list;
    delegate->GetAvailableIMEList(&list);
    IMEPropertyInfoList property_list;
    delegate->GetCurrentIMEProperties(&property_list);
    Update(list, property_list);
  }

  virtual ~IMEDetailedView() {}

  void Update(const IMEInfoList& list,
              const IMEPropertyInfoList& property_list) {
    RemoveAllChildViews(true);

    header_ = NULL;

    AppendHeaderEntry();
    AppendIMEList(list);
    if (!property_list.empty())
      AppendIMEProperties(property_list);
    if (login_ != user::LOGGED_IN_NONE && login_ != user::LOGGED_IN_LOCKED)
      AppendSettings();

    Layout();
    SchedulePaint();
  }

 private:
  void AppendHeaderEntry() {
    header_ = CreateDetailedHeaderEntry(IDS_ASH_STATUS_TRAY_IME, this);
    AddChildView(header_);
  }

  void AppendIMEList(const IMEInfoList& list) {
    views::View* imes = new views::View;
    imes->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 1));
    for (size_t i = 0; i < list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(list[i].name,
          list[i].selected ? gfx::Font::BOLD : gfx::Font::NORMAL);
      imes->AddChildView(container);
      ime_map_[container] = list[i].id;
    }
    imes->set_border(views::Border::CreateSolidSidedBorder(1, 0, 1, 0,
        kBorderLightColor));
    AddChildView(imes);
  }

  void AppendIMEProperties(const IMEPropertyInfoList& property_list) {
    views::View* properties = new views::View;
    properties->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 1));
    for (size_t i = 0; i < property_list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(
          property_list[i].name,
          property_list[i].selected ? gfx::Font::BOLD : gfx::Font::NORMAL);
      properties->AddChildView(container);
      property_map_[container] = property_list[i].key;
    }
    properties->set_border(views::Border::CreateSolidSidedBorder(
        0, 0, 1, 0, kBorderLightColor));
    AddChildView(properties);
  }

  void AppendSettings() {
    HoverHighlightView* container = new HoverHighlightView(this);
    container->AddLabel(ui::ResourceBundle::GetSharedInstance().
        GetLocalizedString(IDS_ASH_STATUS_TRAY_IME_SETTINGS),
        gfx::Font::NORMAL);
    AddChildView(container);
    settings_ = container;
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    if (sender == header_) {
      Shell::GetInstance()->tray()->ShowDefaultView();
    } else if (sender == settings_) {
      delegate->ShowIMESettings();
    } else {
      std::map<views::View*, std::string>::const_iterator ime_find;
      ime_find = ime_map_.find(sender);
      if (ime_find != ime_map_.end()) {
        std::string ime_id = ime_find->second;
        delegate->SwitchIME(ime_id);
        GetWidget()->Close();
      } else {
        std::map<views::View*, std::string>::const_iterator prop_find;
        prop_find = property_map_.find(sender);
        if (prop_find != property_map_.end()) {
          std::string key = prop_find->second;
          delegate->ActivateIMEProperty(key);
        }
      }
    }
  }

  user::LoginStatus login_;

  std::map<views::View*, std::string> ime_map_;
  std::map<views::View*, std::string> property_map_;
  views::View* header_;
  views::View* settings_;

  DISALLOW_COPY_AND_ASSIGN(IMEDetailedView);
};

}  // namespace tray

TrayIME::TrayIME() {
}

TrayIME::~TrayIME() {
}

void TrayIME::UpdateTrayLabel(const IMEInfo& current, size_t count) {
  tray_label_->SetText(current.short_name);
  tray_label_->SetVisible(count > 1);
}

views::View* TrayIME::CreateTrayView(user::LoginStatus status) {
  tray_label_.reset(new views::Label);
  SetupLabelForTray(tray_label_.get());
  tray_label_->set_border(
      views::Border::CreateEmptyBorder(0, 2, 0, 2));
  return tray_label_.get();
}

views::View* TrayIME::CreateDefaultView(user::LoginStatus status) {
  default_.reset(new tray::IMEDefaultView(this));
  return default_.get();
}

views::View* TrayIME::CreateDetailedView(user::LoginStatus status) {
  detailed_.reset(new tray::IMEDetailedView(this, status));
  return detailed_.get();
}

void TrayIME::DestroyTrayView() {
  tray_label_.reset();
}

void TrayIME::DestroyDefaultView() {
  default_.reset();
}

void TrayIME::DestroyDetailedView() {
  detailed_.reset();
}

void TrayIME::OnIMERefresh() {
  SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
  IMEInfoList list;
  IMEInfo current;
  IMEPropertyInfoList property_list;
  delegate->GetCurrentIME(&current);
  delegate->GetAvailableIMEList(&list);
  delegate->GetCurrentIMEProperties(&property_list);

  UpdateTrayLabel(current, list.size());

  if (default_.get())
    default_->UpdateLabel(current);
  if (detailed_.get())
    detailed_->Update(list, property_list);
}

}  // namespace internal
}  // namespace ash
