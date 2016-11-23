// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/ime/tray_ime_chromeos.h"

#include <vector>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/chromeos/ime_menu/ime_list_view.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_item_more.h"
#include "ash/common/system/tray/tray_item_view.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/system/tray_accessibility.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
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
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    HoverHighlightView::GetAccessibleNodeData(node_data);
    node_data->role = ui::AX_ROLE_CHECK_BOX;
    if (selected_)
      node_data->AddStateFlag(ui::AX_STATE_CHECKED);
  }

 private:
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(SelectableHoverHighlightView);
};

class IMEDefaultView : public TrayItemMore {
 public:
  IMEDefaultView(SystemTrayItem* owner, const base::string16& label)
      : TrayItemMore(owner, true) {
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      SetImage(gfx::CreateVectorIcon(kSystemMenuKeyboardIcon, kMenuIconColor));
    } else {
      ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
      SetImage(*bundle.GetImageNamed(IDR_AURA_UBER_TRAY_IME).ToImageSkia());
    }
    UpdateLabel(label);
  }

  ~IMEDefaultView() override {}

  void UpdateLabel(const base::string16& label) {
    SetLabel(label);
    SetAccessibleName(label);
  }

 protected:
  // TrayItemMore:
  void UpdateStyle() override {
    TrayItemMore::UpdateStyle();

    if (!MaterialDesignController::IsSystemTrayMenuMaterial())
      return;

    std::unique_ptr<TrayPopupItemStyle> style = CreateStyle();
    SetImage(
        gfx::CreateVectorIcon(kSystemMenuKeyboardIcon, style->GetIconColor()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IMEDefaultView);
};

class IMEDetailedView : public ImeListView {
 public:
  IMEDetailedView(SystemTrayItem* owner,
                  LoginStatus login,
                  bool show_keyboard_toggle)
      : ImeListView(owner, show_keyboard_toggle, ImeListView::HIDE_SINGLE_IME),
        login_(login),
        settings_(nullptr),
        settings_button_(nullptr) {
    SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
    IMEInfoList list;
    delegate->GetAvailableIMEList(&list);
    IMEPropertyInfoList property_list;
    delegate->GetCurrentIMEProperties(&property_list);
    Update(list, property_list, show_keyboard_toggle,
           ImeListView::HIDE_SINGLE_IME);
  }

  ~IMEDetailedView() override {}

  void Update(const IMEInfoList& list,
              const IMEPropertyInfoList& property_list,
              bool show_keyboard_toggle,
              SingleImeBehavior single_ime_behavior) override {
    ImeListView::Update(list, property_list, show_keyboard_toggle,
                        single_ime_behavior);
    if (!MaterialDesignController::IsSystemTrayMenuMaterial() &&
        TrayPopupUtils::CanOpenWebUISettings(login_)) {
      AppendSettings();
    }

    CreateTitleRow(IDS_ASH_STATUS_TRAY_IME);
  }

 private:
  // ImeListView:
  void HandleViewClicked(views::View* view) override {
    ImeListView::HandleViewClicked(view);
    if (view == settings_)
      ShowSettings();
  }

  void ResetImeListView() override {
    ImeListView::ResetImeListView();
    settings_button_ = nullptr;
  }

  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override {
    ImeListView::HandleButtonPressed(sender, event);
    if (sender == settings_button_)
      ShowSettings();
  }

  void CreateExtraTitleRowButtons() override {
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      tri_view()->SetContainerVisible(TriView::Container::END, true);
      settings_button_ = CreateSettingsButton(login_);
      tri_view()->AddView(TriView::Container::END, settings_button_);
    }
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

  void ShowSettings() {
    WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_IME_SHOW_DETAILED);
    WmShell::Get()->system_tray_controller()->ShowIMESettings();
    if (owner()->system_tray())
      owner()->system_tray()->CloseSystemBubble();
  }

  LoginStatus login_;

  // Not used in material design.
  views::View* settings_;

  // Only used in material design.
  views::Button* settings_button_;

  DISALLOW_COPY_AND_ASSIGN(IMEDetailedView);
};

}  // namespace tray

TrayIME::TrayIME(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_IME),
      tray_label_(NULL),
      default_(NULL),
      detailed_(NULL),
      keyboard_suppressed_(false),
      is_visible_(true) {
  SystemTrayNotifier* tray_notifier = WmShell::Get()->system_tray_notifier();
  tray_notifier->AddVirtualKeyboardObserver(this);
  tray_notifier->AddAccessibilityObserver(this);
  tray_notifier->AddIMEObserver(this);
}

TrayIME::~TrayIME() {
  SystemTrayNotifier* tray_notifier = WmShell::Get()->system_tray_notifier();
  tray_notifier->RemoveIMEObserver(this);
  tray_notifier->RemoveAccessibilityObserver(this);
  tray_notifier->RemoveVirtualKeyboardObserver(this);
}

void TrayIME::OnKeyboardSuppressionChanged(bool suppressed) {
  keyboard_suppressed_ = suppressed;
  Update();
}

void TrayIME::OnAccessibilityModeChanged(
    AccessibilityNotificationVisibility notify) {
  Update();
}

void TrayIME::Update() {
  UpdateTrayLabel(current_ime_, ime_list_.size());
  if (default_) {
    default_->SetVisible(ShouldDefaultViewBeVisible());
    default_->UpdateLabel(GetDefaultViewLabel(ime_list_.size() > 1));
  }
  if (detailed_)
    detailed_->Update(ime_list_, property_list_, ShouldShowKeyboardToggle(),
                      ImeListView::HIDE_SINGLE_IME);
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
         !WmShell::Get()->accessibility_delegate()->IsVirtualKeyboardEnabled();
}

base::string16 TrayIME::GetDefaultViewLabel(bool show_ime_label) {
  if (show_ime_label) {
    IMEInfo current;
    WmShell::Get()->system_tray_delegate()->GetCurrentIME(&current);
    return current.name;
  } else {
    // Display virtual keyboard status instead.
    int id = keyboard::IsKeyboardEnabled()
                 ? IDS_ASH_STATUS_TRAY_KEYBOARD_ENABLED
                 : IDS_ASH_STATUS_TRAY_KEYBOARD_DISABLED;
    return ui::ResourceBundle::GetSharedInstance().GetLocalizedString(id);
  }
}

views::View* TrayIME::CreateTrayView(LoginStatus status) {
  CHECK(tray_label_ == NULL);
  tray_label_ = new TrayItemView(this);
  tray_label_->CreateLabel();
  SetupLabelForTray(tray_label_->label());
  // Hide IME tray when it is created, it will be updated when it is notified
  // of the IME refresh event.
  tray_label_->SetVisible(false);
  return tray_label_;
}

views::View* TrayIME::CreateDefaultView(LoginStatus status) {
  CHECK(default_ == NULL);
  default_ =
      new tray::IMEDefaultView(this, GetDefaultViewLabel(ime_list_.size() > 1));
  default_->SetVisible(ShouldDefaultViewBeVisible());
  return default_;
}

views::View* TrayIME::CreateDetailedView(LoginStatus status) {
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

void TrayIME::UpdateAfterLoginStatusChange(LoginStatus status) {}

void TrayIME::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  SetTrayLabelItemBorder(tray_label_, alignment);
  tray_label_->Layout();
}

void TrayIME::OnIMERefresh() {
  // Caches the current ime state.
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
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
