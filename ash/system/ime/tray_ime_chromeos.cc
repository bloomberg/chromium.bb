// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/tray_ime_chromeos.h"

#include <vector>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray_accessibility.h"
#include "ash/virtual_keyboard_controller.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace tray {

// A |HoverHighlightView| that uses bold or normal font depending on whether
// it is selected.  This view exposes itself as a checkbox to the accessibility
// framework.
class SelectableHoverHighlightView : public HoverHighlightView {
 public:
  SelectableHoverHighlightView(ViewClickListener* listener,
                               const base::string16& label,
                               bool selected)
      : HoverHighlightView(listener), selected_(selected) {
    AddLabel(label, gfx::ALIGN_LEFT, selected);
  }

  ~SelectableHoverHighlightView() override {}

 protected:
  // Overridden from views::View.
  void GetAccessibleState(ui::AXViewState* state) override {
    HoverHighlightView::GetAccessibleState(state);
    state->role = ui::AX_ROLE_CHECK_BOX;
    if (selected_)
      state->AddStateFlag(ui::AX_STATE_CHECKED);
  }

 private:
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(SelectableHoverHighlightView);
};

class IMEDefaultView : public TrayItemMore {
 public:
  explicit IMEDefaultView(SystemTrayItem* owner, const base::string16& label)
      : TrayItemMore(owner, true) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_IME).ToImageSkia());
    UpdateLabel(label);
  }

  ~IMEDefaultView() override {}

  void UpdateLabel(const base::string16& label) {
    SetLabel(label);
    SetAccessibleName(label);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IMEDefaultView);
};

class IMEDetailedView : public TrayDetailsView, public ViewClickListener {
 public:
  IMEDetailedView(SystemTrayItem* owner,
                  user::LoginStatus login,
                  bool show_keyboard_toggle)
      : TrayDetailsView(owner), login_(login) {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    IMEInfoList list;
    delegate->GetAvailableIMEList(&list);
    IMEPropertyInfoList property_list;
    delegate->GetCurrentIMEProperties(&property_list);
    Update(list, property_list, show_keyboard_toggle);
  }

  ~IMEDetailedView() override {}

  void Update(const IMEInfoList& list,
              const IMEPropertyInfoList& property_list,
              bool show_keyboard_toggle) {
    Reset();
    ime_map_.clear();
    property_map_.clear();
    CreateScrollableList();

    if (list.size() > 1)
      AppendIMEList(list);
    if (!property_list.empty())
      AppendIMEProperties(property_list);

    if (show_keyboard_toggle) {
      if (list.size() > 1 || !property_list.empty())
        AddScrollSeparator();
      AppendKeyboardStatus();
    }

    bool userAddingRunning = ash::Shell::GetInstance()
                                 ->session_state_delegate()
                                 ->IsInSecondaryLoginScreen();
    if (login_ != user::LOGGED_IN_NONE && login_ != user::LOGGED_IN_LOCKED &&
        !userAddingRunning)
      AppendSettings();
    AppendHeaderEntry();

    Layout();
    SchedulePaint();
  }

 private:
  void AppendHeaderEntry() { CreateSpecialRow(IDS_ASH_STATUS_TRAY_IME, this); }

  // Appends the IMEs to the scrollable area of the detailed view.
  void AppendIMEList(const IMEInfoList& list) {
    DCHECK(ime_map_.empty());
    for (size_t i = 0; i < list.size(); i++) {
      HoverHighlightView* container = new SelectableHoverHighlightView(
          this, list[i].name, list[i].selected);
      scroll_content()->AddChildView(container);
      ime_map_[container] = list[i].id;
    }
  }

  // Appends the IME listed to the scrollable area of the detailed
  // view.
  void AppendIMEProperties(const IMEPropertyInfoList& property_list) {
    DCHECK(property_map_.empty());
    for (size_t i = 0; i < property_list.size(); i++) {
      HoverHighlightView* container = new SelectableHoverHighlightView(
          this, property_list[i].name, property_list[i].selected);
      if (i == 0)
        container->SetBorder(views::Border::CreateSolidSidedBorder(
            1, 0, 0, 0, kBorderLightColor));
      scroll_content()->AddChildView(container);
      property_map_[container] = property_list[i].key;
    }
  }

  void AppendKeyboardStatus() {
    HoverHighlightView* container = new HoverHighlightView(this);
    int id = keyboard::IsKeyboardEnabled()
                 ? IDS_ASH_STATUS_TRAY_DISABLE_KEYBOARD
                 : IDS_ASH_STATUS_TRAY_ENABLE_KEYBOARD;
    container->AddLabel(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(id),
        gfx::ALIGN_LEFT, false /* highlight */);
    scroll_content()->AddChildView(container);
    keyboard_status_ = container;
  }

  void AppendSettings() {
    HoverHighlightView* container = new HoverHighlightView(this);
    container->AddLabel(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_IME_SETTINGS),
        gfx::ALIGN_LEFT, false /* highlight */);
    AddChildView(container);
    settings_ = container;
  }

  // Overridden from ViewClickListener.
  void OnViewClicked(views::View* sender) override {
    SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
    if (sender == footer()->content()) {
      TransitionToDefaultView();
    } else if (sender == settings_) {
      Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          ash::UMA_STATUS_AREA_IME_SHOW_DETAILED);
      delegate->ShowIMESettings();
    } else if (sender == keyboard_status_) {
      Shell::GetInstance()->virtual_keyboard_controller()
          ->ToggleIgnoreExternalKeyboard();
    } else {
      std::map<views::View*, std::string>::const_iterator ime_find;
      ime_find = ime_map_.find(sender);
      if (ime_find != ime_map_.end()) {
        Shell::GetInstance()->metrics()->RecordUserMetricsAction(
            ash::UMA_STATUS_AREA_IME_SWITCH_MODE);
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
  views::View* keyboard_status_;

  DISALLOW_COPY_AND_ASSIGN(IMEDetailedView);
};

}  // namespace tray

TrayIME::TrayIME(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_label_(NULL),
      default_(NULL),
      detailed_(NULL),
      keyboard_suppressed_(false),
      is_visible_(true) {
  Shell::GetInstance()->system_tray_notifier()->AddIMEObserver(this);
  Shell::GetInstance()->system_tray_notifier()->AddVirtualKeyboardObserver(
      this);
  Shell::GetInstance()->system_tray_notifier()->AddAccessibilityObserver(this);
}

TrayIME::~TrayIME() {
  Shell::GetInstance()->system_tray_notifier()->RemoveIMEObserver(this);
  Shell::GetInstance()->system_tray_notifier()->RemoveAccessibilityObserver(
      this);
  Shell::GetInstance()->system_tray_notifier()->RemoveVirtualKeyboardObserver(
      this);
}

void TrayIME::OnKeyboardSuppressionChanged(bool suppressed) {
  keyboard_suppressed_ = suppressed;
  Update();
}

void TrayIME::OnAccessibilityModeChanged(
    ui::AccessibilityNotificationVisibility notify) {
  Update();
}

void TrayIME::Update() {
  UpdateTrayLabel(current_ime_, ime_list_.size());
  if (default_) {
    default_->SetVisible(ShouldDefaultViewBeVisible());
    default_->UpdateLabel(GetDefaultViewLabel(ime_list_.size() > 1));
  }
  if (detailed_)
    detailed_->Update(ime_list_, property_list_, ShouldShowKeyboardToggle());
}

void TrayIME::UpdateTrayLabel(const IMEInfo& current, size_t count) {
  if (tray_label_) {
    bool visible = count > 1 && is_visible_;
    tray_label_->SetVisible(visible);
    // Do not change label before hiding because this change is noticeable.
    if (!visible)
      return;
    if (current.third_party) {
      tray_label_->label()->SetText(current.short_name +
                                    base::UTF8ToUTF16("*"));
    } else {
      tray_label_->label()->SetText(current.short_name);
    }
    SetTrayLabelItemBorder(tray_label_, system_tray()->shelf_alignment());
    tray_label_->Layout();
  }
}

bool TrayIME::ShouldShowKeyboardToggle() {
  return keyboard_suppressed_ &&
         !Shell::GetInstance()
              ->accessibility_delegate()
              ->IsVirtualKeyboardEnabled();
}

base::string16 TrayIME::GetDefaultViewLabel(bool show_ime_label) {
  if (show_ime_label) {
    IMEInfo current;
    Shell::GetInstance()->system_tray_delegate()->GetCurrentIME(&current);
    return current.name;
  } else {
    // Display virtual keyboard status instead.
    int id = keyboard::IsKeyboardEnabled()
                 ? IDS_ASH_STATUS_TRAY_KEYBOARD_ENABLED
                 : IDS_ASH_STATUS_TRAY_KEYBOARD_DISABLED;
    return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(id);
  }
}

views::View* TrayIME::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_label_ == NULL);
  tray_label_ = new TrayItemView(this);
  tray_label_->CreateLabel();
  SetupLabelForTray(tray_label_->label());
  // Hide IME tray when it is created, it will be updated when it is notified
  // of the IME refresh event.
  tray_label_->SetVisible(false);
  return tray_label_;
}

views::View* TrayIME::CreateDefaultView(user::LoginStatus status) {
  CHECK(default_ == NULL);
  default_ =
      new tray::IMEDefaultView(this, GetDefaultViewLabel(ime_list_.size() > 1));
  default_->SetVisible(ShouldDefaultViewBeVisible());
  return default_;
}

views::View* TrayIME::CreateDetailedView(user::LoginStatus status) {
  CHECK(detailed_ == NULL);
  detailed_ =
      new tray::IMEDetailedView(this, status, ShouldShowKeyboardToggle());
  return detailed_;
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

void TrayIME::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayIME::UpdateAfterShelfAlignmentChange(wm::ShelfAlignment alignment) {
  SetTrayLabelItemBorder(tray_label_, alignment);
  tray_label_->Layout();
}

void TrayIME::OnIMERefresh() {
  // Caches the current ime state.
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  ime_list_.clear();
  property_list_.clear();
  delegate->GetCurrentIME(&current_ime_);
  delegate->GetAvailableIMEList(&ime_list_);
  delegate->GetCurrentIMEProperties(&property_list_);

  Update();
}

void TrayIME::OnIMEMenuActivationChanged(bool is_active) {
  is_visible_ = !is_active;
  if (is_visible_)
    OnIMERefresh();
  else
    Update();
}

bool TrayIME::ShouldDefaultViewBeVisible() {
  return is_visible_ && (ime_list_.size() > 1 || property_list_.size() > 1 ||
                         ShouldShowKeyboardToggle());
}

}  // namespace ash
