// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_caps_lock.h"

#include "ash/accessibility_delegate.h"
#include "ash/resources/grit/ash_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/sys_info.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

using message_center::Notification;

namespace ash {
namespace {

// Padding used to position the caption in the caps lock default view row.
const int kCaptionRightPadding = 6;

const char kCapsLockNotificationId[] = "capslock";

bool CapsLockIsEnabled() {
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  return (ime && ime->GetImeKeyboard())
             ? ime->GetImeKeyboard()->CapsLockIsEnabled()
             : false;
}

std::unique_ptr<Notification> CreateNotification() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  const int string_id =
      Shell::Get()->system_tray_delegate()->IsSearchKeyMappedToCapsLock()
          ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_SEARCH
          : IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_ALT_SEARCH;
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kCapsLockNotificationId,
      base::string16(), bundle.GetLocalizedString(string_id),
      gfx::Image(
          gfx::CreateVectorIcon(kSystemMenuCapsLockIcon,
                                TrayPopupItemStyle::GetIconColor(
                                    TrayPopupItemStyle::ColorStyle::ACTIVE))),
      base::string16() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 system_notifier::kNotifierCapsLock),
      message_center::RichNotificationData(), nullptr));
  return notification;
}

}  // namespace

class CapsLockDefaultView : public ActionableView {
 public:
  CapsLockDefaultView()
      : ActionableView(nullptr, TrayPopupInkDropStyle::FILL_BOUNDS),
        text_label_(TrayPopupUtils::CreateDefaultLabel()),
        shortcut_label_(TrayPopupUtils::CreateDefaultLabel()) {
    shortcut_label_->SetEnabled(false);

    TriView* tri_view(TrayPopupUtils::CreateDefaultRowView());
    SetLayoutManager(new views::FillLayout);
    AddChildView(tri_view);

    auto* image = TrayPopupUtils::CreateMainImageView();
    image->SetEnabled(enabled());
    TrayPopupItemStyle default_view_style(
        TrayPopupItemStyle::FontStyle::DEFAULT_VIEW_LABEL);
    image->SetImage(gfx::CreateVectorIcon(kSystemMenuCapsLockIcon,
                                          default_view_style.GetIconColor()));
    default_view_style.SetupLabel(text_label_);

    TrayPopupItemStyle caption_style(TrayPopupItemStyle::FontStyle::CAPTION);
    caption_style.SetupLabel(shortcut_label_);

    SetInkDropMode(InkDropHostView::InkDropMode::ON);

    tri_view->AddView(TriView::Container::START, image);
    tri_view->AddView(TriView::Container::CENTER, text_label_);
    tri_view->AddView(TriView::Container::END, shortcut_label_);
    tri_view->SetContainerBorder(
        TriView::Container::END,
        views::CreateEmptyBorder(
            0, 0, 0, kCaptionRightPadding + kTrayPopupLabelRightPadding));
  }

  ~CapsLockDefaultView() override {}

  // Updates the label text and the shortcut text.
  void Update(bool caps_lock_enabled) {
    const int text_string_id = caps_lock_enabled
                                   ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_ENABLED
                                   : IDS_ASH_STATUS_TRAY_CAPS_LOCK_DISABLED;
    text_label_->SetText(l10n_util::GetStringUTF16(text_string_id));

    int shortcut_string_id = 0;
    bool search_mapped_to_caps_lock =
        Shell::Get()->system_tray_delegate()->IsSearchKeyMappedToCapsLock();
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
    shortcut_label_->SetText(l10n_util::GetStringUTF16(shortcut_string_id));

    Layout();
  }

 private:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->role = ui::AX_ROLE_BUTTON;
    node_data->SetName(text_label_->text());
  }

  // ActionableView:
  bool PerformAction(const ui::Event& event) override {
    chromeos::input_method::ImeKeyboard* keyboard =
        chromeos::input_method::InputMethodManager::Get()->GetImeKeyboard();
    if (keyboard) {
      ShellPort::Get()->RecordUserMetricsAction(
          keyboard->CapsLockIsEnabled()
              ? UMA_STATUS_AREA_CAPS_LOCK_DISABLED_BY_CLICK
              : UMA_STATUS_AREA_CAPS_LOCK_ENABLED_BY_CLICK);
      keyboard->SetCapsLockEnabled(!keyboard->CapsLockIsEnabled());
    }
    return true;
  }

  // It indicates whether the Caps Lock is on or off.
  views::Label* text_label_;

  // It indicates the shortcut can be used to turn on or turn off Caps Lock.
  views::Label* shortcut_label_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockDefaultView);
};

TrayCapsLock::TrayCapsLock(SystemTray* system_tray)
    : TrayImageItem(system_tray, kSystemTrayCapsLockIcon, UMA_CAPS_LOCK),
      default_(nullptr),
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
  Shell::Get()->accessibility_delegate()->TriggerAccessibilityAlert(
      enabled ? A11Y_ALERT_CAPS_ON : A11Y_ALERT_CAPS_OFF);

  if (tray_view())
    tray_view()->SetVisible(caps_lock_enabled_);

  if (default_) {
    default_->Update(caps_lock_enabled_);
  } else {
    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();
    if (caps_lock_enabled_) {
      if (!message_shown_) {
        ShellPort::Get()->RecordUserMetricsAction(
            UMA_STATUS_AREA_CAPS_LOCK_POPUP);

        message_center->AddNotification(CreateNotification());
        message_shown_ = true;
      }
    } else if (message_center->FindVisibleNotificationById(
                   kCapsLockNotificationId)) {
      message_center->RemoveNotification(kCapsLockNotificationId, false);
    }
  }
}

bool TrayCapsLock::GetInitialVisibility() {
  return CapsLockIsEnabled();
}

views::View* TrayCapsLock::CreateDefaultView(LoginStatus status) {
  if (!caps_lock_enabled_)
    return nullptr;
  DCHECK(!default_);
  default_ = new CapsLockDefaultView;
  default_->Update(caps_lock_enabled_);
  return default_;
}

void TrayCapsLock::DestroyDefaultView() {
  default_ = nullptr;
}

}  // namespace ash
