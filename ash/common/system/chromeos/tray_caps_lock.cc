// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/tray_caps_lock.h"

#include "ash/common/accessibility_delegate.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/actionable_view.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/wm_shell.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/sys_info.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Padding used to position the caption in the caps lock default view row.
const int kCaptionRightPadding = 6;

bool CapsLockIsEnabled() {
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  return (ime && ime->GetImeKeyboard())
             ? ime->GetImeKeyboard()->CapsLockIsEnabled()
             : false;
}
}

class CapsLockDefaultView : public ActionableView {
 public:
  CapsLockDefaultView()
      : ActionableView(nullptr, TrayPopupInkDropStyle::FILL_BOUNDS),
        image_(TrayPopupUtils::CreateMainImageView()),
        text_label_(TrayPopupUtils::CreateDefaultLabel()),
        shortcut_label_(TrayPopupUtils::CreateDefaultLabel()) {
    shortcut_label_->SetEnabled(false);

    TriView* tri_view(TrayPopupUtils::CreateDefaultRowView());
    SetLayoutManager(new views::FillLayout);
    AddChildView(tri_view);

    if (MaterialDesignController::UseMaterialDesignSystemIcons()) {
      image_->SetEnabled(enabled());
      UpdateStyle();
      SetInkDropMode(InkDropHostView::InkDropMode::ON);
    } else {
      ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
      image_->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_CAPS_LOCK_DARK)
                           .ToImageSkia());
    }
    tri_view->AddView(TriView::Container::START, image_);
    tri_view->AddView(TriView::Container::CENTER, text_label_);
    tri_view->AddView(TriView::Container::END, shortcut_label_);
    tri_view->SetContainerBorder(
        TriView::Container::END,
        views::CreateEmptyBorder(0, 0, 0, kCaptionRightPadding));
  }

  ~CapsLockDefaultView() override {}

  // Updates the label text and the shortcut text.
  void Update(bool caps_lock_enabled) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    const int text_string_id = caps_lock_enabled
                                   ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_ENABLED
                                   : IDS_ASH_STATUS_TRAY_CAPS_LOCK_DISABLED;
    text_label_->SetText(bundle.GetLocalizedString(text_string_id));

    int shortcut_string_id = 0;
    bool search_mapped_to_caps_lock =
        WmShell::Get()->system_tray_delegate()->IsSearchKeyMappedToCapsLock();
    if (caps_lock_enabled) {
      shortcut_string_id =
          search_mapped_to_caps_lock
              ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_SEARCH_OR_SHIFT
              : IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_ALT_SEARCH_OR_SHIFT;
    } else {
      shortcut_string_id =
          search_mapped_to_caps_lock
              ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_SEARCH
              : IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_ALT_SEARCH;
    }
    shortcut_label_->SetText(bundle.GetLocalizedString(shortcut_string_id));

    UpdateStyle();
    Layout();
  }

  // ActionableView:
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override {
    ActionableView::OnNativeThemeChanged(theme);
    UpdateStyle();
  }

 private:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ui::AX_ROLE_BUTTON;
    node_data->SetName(text_label_->text());
  }

  // Update the Text theme and style based on the current theme.
  void UpdateStyle() {
    TrayPopupItemStyle default_view_style(
        GetNativeTheme(), TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
    // Set image and label styles for Material Design Caps Lock default view.
    image_->SetImage(gfx::CreateVectorIcon(kSystemMenuCapsLockIcon,
                                           default_view_style.GetIconColor()));
    default_view_style.SetupLabel(text_label_);
    TrayPopupItemStyle caption_style(GetNativeTheme(),
                                     TrayPopupItemStyle::FontStyle::CAPTION);
    caption_style.SetupLabel(shortcut_label_);
  }

  // ActionableView:
  bool PerformAction(const ui::Event& event) override {
    chromeos::input_method::ImeKeyboard* keyboard =
        chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
    if (keyboard) {
      WmShell::Get()->RecordUserMetricsAction(
          keyboard->CapsLockIsEnabled()
              ? UMA_STATUS_AREA_CAPS_LOCK_DISABLED_BY_CLICK
              : UMA_STATUS_AREA_CAPS_LOCK_ENABLED_BY_CLICK);
      keyboard->SetCapsLockEnabled(!keyboard->CapsLockIsEnabled());
    }
    return true;
  }

  // It contains the image represents the Caps Lock.
  views::ImageView* image_;

  // It indicates whether the Caps Lock is on or off.
  views::Label* text_label_;

  // It indicates the shortcut can be used to turn on or turn off Caps Lock.
  views::Label* shortcut_label_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockDefaultView);
};

TrayCapsLock::TrayCapsLock(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_CAPS_LOCK, UMA_CAPS_LOCK),
      default_(NULL),
      detailed_(NULL),
      caps_lock_enabled_(CapsLockIsEnabled()),
      message_shown_(false) {
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  if (ime && ime->GetImeKeyboard())
    ime->GetImeKeyboard()->AddObserver(this);
}

TrayCapsLock::~TrayCapsLock() {
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  if (ime && ime->GetImeKeyboard())
    ime->GetImeKeyboard()->RemoveObserver(this);
}

void TrayCapsLock::OnCapsLockChanged(bool enabled) {
  caps_lock_enabled_ = enabled;

  // Send an a11y alert.
  WmShell::Get()->accessibility_delegate()->TriggerAccessibilityAlert(
      enabled ? A11Y_ALERT_CAPS_ON : A11Y_ALERT_CAPS_OFF);

  if (tray_view())
    tray_view()->SetVisible(caps_lock_enabled_);

  if (default_) {
    default_->Update(caps_lock_enabled_);
  } else {
    if (caps_lock_enabled_) {
      if (!message_shown_) {
        WmShell::Get()->RecordUserMetricsAction(
            UMA_STATUS_AREA_CAPS_LOCK_POPUP);
        PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
        message_shown_ = true;
      }
    } else if (detailed_) {
      detailed_->GetWidget()->Close();
    }
  }
}

bool TrayCapsLock::GetInitialVisibility() {
  return CapsLockIsEnabled();
}

views::View* TrayCapsLock::CreateDefaultView(LoginStatus status) {
  if (!caps_lock_enabled_)
    return NULL;
  DCHECK(default_ == NULL);
  default_ = new CapsLockDefaultView;
  default_->Update(caps_lock_enabled_);
  return default_;
}

views::View* TrayCapsLock::CreateDetailedView(LoginStatus status) {
  DCHECK(detailed_ == NULL);
  detailed_ = new views::View;

  detailed_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, kTrayPopupPaddingHorizontal, 10,
      kTrayPopupPaddingBetweenItems));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  views::ImageView* image = new views::ImageView;
  if (MaterialDesignController::UseMaterialDesignSystemIcons()) {
    image->SetImage(CreateVectorIcon(kSystemMenuCapsLockIcon, kMenuIconSize,
                                     kMenuIconColor));
  } else {
    image->SetImage(
        bundle.GetImageNamed(IDR_AURA_UBER_TRAY_CAPS_LOCK_DARK).ToImageSkia());
  }

  detailed_->AddChildView(image);

  const int string_id =
      WmShell::Get()->system_tray_delegate()->IsSearchKeyMappedToCapsLock()
          ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_SEARCH
          : IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_ALT_SEARCH;
  views::Label* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(bundle.GetLocalizedString(string_id));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  detailed_->AddChildView(label);
  WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_CAPS_LOCK_DETAILED);

  return detailed_;
}

void TrayCapsLock::DestroyDefaultView() {
  default_ = NULL;
}

void TrayCapsLock::DestroyDetailedView() {
  detailed_ = NULL;
}

}  // namespace ash
