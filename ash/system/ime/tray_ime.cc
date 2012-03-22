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
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
        kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    views::ImageView* icon = new views::ImageView;
    icon->SetImage(bundle.GetImageNamed(
        IDR_AURA_UBER_TRAY_IME).ToSkBitmap());
    AddChildView(icon);

    label_ = new views::Label;
    AddChildView(label_);

    IMEInfo info;
    Shell::GetInstance()->tray_delegate()->GetCurrentIME(&info);
    UpdateLabel(info);

    AddMore();
  }

  virtual ~IMEDefaultView() {}

  void UpdateLabel(const IMEInfo& info) {
    label_->SetText(info.name);
  }

 private:
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(IMEDefaultView);
};

class IMEDetailedView : public views::View,
                        public ViewClickListener {
 public:
  explicit IMEDetailedView(SystemTrayItem* owner)
      : header_(NULL) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 1, 1, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
    IMEInfoList list;
    Shell::GetInstance()->tray_delegate()->GetAvailableIMEList(&list);
    Update(list);
  }

  virtual ~IMEDetailedView() {}

  void Update(const IMEInfoList& list) {
    RemoveAllChildViews(true);

    header_ = NULL;

    AppendHeaderEntry();
    AppendIMEList(list);
    AppendSettings();

    Layout();
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
      std::map<views::View*, std::string>::iterator find;
      find = ime_map_.find(sender);
      if (find != ime_map_.end()) {
        std::string ime_id = find->second;
        delegate->SwitchIME(ime_id);
      }
    }
  }

  std::map<views::View*, std::string> ime_map_;
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
  return tray_label_.get();
}

views::View* TrayIME::CreateDefaultView(user::LoginStatus status) {
  default_.reset(new tray::IMEDefaultView(this));
  return default_.get();
}

views::View* TrayIME::CreateDetailedView(user::LoginStatus status) {
  detailed_.reset(new tray::IMEDetailedView(this));
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
  delegate->GetCurrentIME(&current);
  delegate->GetAvailableIMEList(&list);

  UpdateTrayLabel(current, list.size());

  if (default_.get())
    default_->UpdateLabel(current);
  if (detailed_.get())
    detailed_->Update(list);
}

}  // namespace internal
}  // namespace ash
