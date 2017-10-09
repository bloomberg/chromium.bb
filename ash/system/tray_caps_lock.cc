// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_caps_lock.h"

#include <memory>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/ime/ime_controller.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/sys_info.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/events/modifier_key.h"
#include "ui/chromeos/events/pref_names.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/public/cpp/message_center_switches.h"
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

bool IsCapsLockEnabled() {
  return Shell::Get()->ime_controller()->IsCapsLockEnabled();
}

bool IsSearchKeyMappedToCapsLock() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  // Null early in mash startup.
  if (!prefs)
    return false;

  // Don't bother to observe for the pref changing because the system tray
  // menu is rebuilt every time it is opened and the user has to close the
  // menu to open settings to change the pref. It's not worth the complexity
  // to worry about sync changing the pref while the menu or notification is
  // visible.
  return prefs->GetInteger(prefs::kLanguageRemapSearchKeyTo) ==
         ui::chromeos::ModifierKey::kCapsLockKey;
}

std::unique_ptr<Notification> CreateNotification() {
  const int string_id =
      IsSearchKeyMappedToCapsLock()
          ? IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_SEARCH
          : IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_ALT_SEARCH;
  std::unique_ptr<Notification> notification;
  if (message_center::IsNewStyleNotificationEnabled()) {
    notification = Notification::CreateSystemNotification(
        message_center::NOTIFICATION_TYPE_SIMPLE, kCapsLockNotificationId,
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAPS_LOCK_ENABLED),
        l10n_util::GetStringUTF16(string_id), gfx::Image(),
        base::string16() /* display_source */, GURL(),
        message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                   system_notifier::kNotifierCapsLock),
        message_center::RichNotificationData(), nullptr,
        kNotificationCapslockIcon,
        message_center::SystemNotificationWarningLevel::NORMAL);
  } else {
    notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, kCapsLockNotificationId,
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAPS_LOCK_ENABLED),
        l10n_util::GetStringUTF16(string_id),
        gfx::Image(
            gfx::CreateVectorIcon(kSystemMenuCapsLockIcon,
                                  TrayPopupItemStyle::GetIconColor(
                                      TrayPopupItemStyle::ColorStyle::ACTIVE))),
        base::string16() /* display_source */, GURL(),
        message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                   system_notifier::kNotifierCapsLock),
        message_center::RichNotificationData(), nullptr);
  }
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
    const bool search_mapped_to_caps_lock = IsSearchKeyMappedToCapsLock();
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
    bool new_state = !IsCapsLockEnabled();
    Shell::Get()->ime_controller()->SetCapsLockFromTray(new_state);
    Shell::Get()->metrics()->RecordUserMetricsAction(
        new_state ? UMA_STATUS_AREA_CAPS_LOCK_ENABLED_BY_CLICK
                  : UMA_STATUS_AREA_CAPS_LOCK_DISABLED_BY_CLICK);
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
      caps_lock_enabled_(IsCapsLockEnabled()),
      message_shown_(false) {
  Shell::Get()->ime_controller()->AddObserver(this);
}

TrayCapsLock::~TrayCapsLock() {
  Shell::Get()->ime_controller()->RemoveObserver(this);
}

// static
void TrayCapsLock::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                        bool for_test) {
  if (for_test) {
    // There is no remote pref service, so pretend that ash owns the pref.
    registry->RegisterIntegerPref(prefs::kLanguageRemapSearchKeyTo,
                                  ui::chromeos::ModifierKey::kSearchKey);
    return;
  }
  // Pref is owned by chrome and flagged as PUBLIC.
  registry->RegisterForeignPref(prefs::kLanguageRemapSearchKeyTo);
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
        Shell::Get()->metrics()->RecordUserMetricsAction(
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
  return IsCapsLockEnabled();
}

views::View* TrayCapsLock::CreateDefaultView(LoginStatus status) {
  if (!caps_lock_enabled_)
    return nullptr;
  DCHECK(!default_);
  default_ = new CapsLockDefaultView;
  default_->Update(caps_lock_enabled_);
  return default_;
}

void TrayCapsLock::OnDefaultViewDestroyed() {
  default_ = nullptr;
}

}  // namespace ash
