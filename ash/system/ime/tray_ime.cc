// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/tray_ime.h"

#include <vector>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_views.h"
#include "ash/wm/shelf_layout_manager.h"
#include "base/logging.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace tray {

class IMEDefaultView : public TrayItemMore {
 public:
  explicit IMEDefaultView(SystemTrayItem* owner)
      : TrayItemMore(owner, true) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    SetImage(bundle.GetImageNamed(
        IDR_AURA_UBER_TRAY_IME).ToImageSkia());

    IMEInfo info;
    Shell::GetInstance()->system_tray_delegate()->GetCurrentIME(&info);
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

class IMEDetailedView : public TrayDetailsView,
                        public ViewClickListener {
 public:
  IMEDetailedView(SystemTrayItem* owner, user::LoginStatus login)
      : TrayDetailsView(owner),
        login_(login) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    IMEInfoList list;
    delegate->GetAvailableIMEList(&list);
    IMEPropertyInfoList property_list;
    delegate->GetCurrentIMEProperties(&property_list);
    Update(list, property_list);
  }

  virtual ~IMEDetailedView() {}

  void Update(const IMEInfoList& list,
              const IMEPropertyInfoList& property_list) {
    Reset();

    AppendIMEList(list);
    if (!property_list.empty())
      AppendIMEProperties(property_list);
    if (login_ != user::LOGGED_IN_NONE && login_ != user::LOGGED_IN_LOCKED)
      AppendSettings();
    AppendHeaderEntry();

    Layout();
    SchedulePaint();
  }

 private:
  void AppendHeaderEntry() {
    CreateSpecialRow(IDS_ASH_STATUS_TRAY_IME, this);
  }

  void AppendIMEList(const IMEInfoList& list) {
    ime_map_.clear();
    CreateScrollableList();
    for (size_t i = 0; i < list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(list[i].name,
          list[i].selected ? gfx::Font::BOLD : gfx::Font::NORMAL);
      scroll_content()->AddChildView(container);
      ime_map_[container] = list[i].id;
    }
  }

  void AppendIMEProperties(const IMEPropertyInfoList& property_list) {
    property_map_.clear();
    for (size_t i = 0; i < property_list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(
          property_list[i].name,
          property_list[i].selected ? gfx::Font::BOLD : gfx::Font::NORMAL);
      if (i == 0)
        container->set_border(views::Border::CreateSolidSidedBorder(1, 0, 0, 0,
        kBorderLightColor));
      scroll_content()->AddChildView(container);
      property_map_[container] = property_list[i].key;
    }
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
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    if (sender == footer()->content()) {
      owner()->system_tray()->ShowDefaultView(BUBBLE_USE_EXISTING);
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
          const std::string key = prop_find->second;
          delegate->ActivateIMEProperty(key);
          GetWidget()->Close();
        }
      }
    }
  }

  user::LoginStatus login_;

  std::map<views::View*, std::string> ime_map_;
  std::map<views::View*, std::string> property_map_;
  views::View* settings_;

  DISALLOW_COPY_AND_ASSIGN(IMEDetailedView);
};

class IMENotificationView : public TrayNotificationView {
 public:
  explicit IMENotificationView(TrayIME* owner)
      : TrayNotificationView(owner, IDR_AURA_UBER_TRAY_IME) {
    InitView(GetLabel());
  }

  void UpdateLabel() {
    RestartAutoCloseTimer();
    UpdateView(GetLabel());
  }

  void StartAutoCloseTimer(int seconds) {
    autoclose_.Stop();
    autoclose_delay_ = seconds;
    if (autoclose_delay_) {
      autoclose_.Start(FROM_HERE,
                       base::TimeDelta::FromSeconds(autoclose_delay_),
                       this, &IMENotificationView::Close);
    }
  }

  void StopAutoCloseTimer() {
    autoclose_.Stop();
  }

  void RestartAutoCloseTimer() {
    if (autoclose_delay_)
      StartAutoCloseTimer(autoclose_delay_);
  }

  // Overridden from TrayNotificationView.
  virtual void OnClickAction() OVERRIDE {
    owner()->PopupDetailedView(0, true);
  }

 private:
  void Close() {
    owner()->HideNotificationView();
  }

  views::Label* GetLabel() {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    IMEInfo current;
    delegate->GetCurrentIME(&current);

    // TODO(zork): Use IDS_ASH_STATUS_TRAY_THIRD_PARTY_IME_TURNED_ON_BUBBLE for
    // third party IMEs
    views::Label* label = new views::Label(
        l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_IME_TURNED_ON_BUBBLE,
            current.medium_name));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    return label;
  }


  int autoclose_delay_;
  base::OneShotTimer<IMENotificationView> autoclose_;

  DISALLOW_COPY_AND_ASSIGN(IMENotificationView);
};


}  // namespace tray

TrayIME::TrayIME(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_label_(NULL),
      default_(NULL),
      detailed_(NULL),
      notification_(NULL),
      message_shown_(false) {
  Shell::GetInstance()->system_tray_notifier()->AddIMEObserver(this);
}

TrayIME::~TrayIME() {
  Shell::GetInstance()->system_tray_notifier()->RemoveIMEObserver(this);
}

void TrayIME::UpdateTrayLabel(const IMEInfo& current, size_t count) {
  if (tray_label_) {
    if (current.third_party) {
      tray_label_->label()->SetText(current.short_name + UTF8ToUTF16("*"));
    } else {
      tray_label_->label()->SetText(current.short_name);
    }
    tray_label_->SetVisible(count > 1);
    SetTrayLabelItemBorder(tray_label_, system_tray()->shelf_alignment());
    tray_label_->Layout();
  }
}

views::View* TrayIME::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_label_ == NULL);
  tray_label_ = new TrayItemView(this);
  tray_label_->CreateLabel();
  SetupLabelForTray(tray_label_->label());
  return tray_label_;
}

views::View* TrayIME::CreateDefaultView(user::LoginStatus status) {
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  IMEInfoList list;
  IMEPropertyInfoList property_list;
  delegate->GetAvailableIMEList(&list);
  delegate->GetCurrentIMEProperties(&property_list);
  if (list.size() <= 1 && property_list.size() <= 1)
    return NULL;
  CHECK(default_ == NULL);
  default_ = new tray::IMEDefaultView(this);
  return default_;
}

views::View* TrayIME::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  detailed_ = new tray::IMEDetailedView(this, status);
  return detailed_;
}

views::View* TrayIME::CreateNotificationView(user::LoginStatus status) {
  DCHECK(notification_ == NULL);
  notification_ = new tray::IMENotificationView(this);
  notification_->StartAutoCloseTimer(kTrayPopupAutoCloseDelayForTextInSeconds);
  return notification_;
}

void TrayIME::DestroyTrayView() {
  tray_label_ = NULL;
}

void TrayIME::DestroyDefaultView() {
  default_ = NULL;
}

void TrayIME::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayIME::DestroyNotificationView() {
  notification_ = NULL;
}

void TrayIME::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayIME::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  SetTrayLabelItemBorder(tray_label_, alignment);
}

void TrayIME::OnIMERefresh(bool show_message) {
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  IMEInfoList list;
  IMEInfo current;
  IMEPropertyInfoList property_list;
  delegate->GetCurrentIME(&current);
  delegate->GetAvailableIMEList(&list);
  delegate->GetCurrentIMEProperties(&property_list);

  UpdateTrayLabel(current, list.size());

  if (default_)
    default_->UpdateLabel(current);
  if (detailed_)
    detailed_->Update(list, property_list);

  if (list.size() > 1 && show_message) {
    // If the notification is still visible, hide it and clear the flag so it is
    // refreshed.
    if (notification_) {
      notification_->UpdateLabel();
    } else if (!Shell::GetPrimaryRootWindowController()->shelf()->IsVisible() ||
               !message_shown_) {
      ShowNotificationView();
      message_shown_ = true;
    }
  }
}

}  // namespace internal
}  // namespace ash
