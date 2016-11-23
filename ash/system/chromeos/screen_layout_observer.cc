// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/screen_layout_observer.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/system/chromeos/devicetype_utils.h"
#include "ash/common/system/system_notifier.h"
#include "ash/common/system/tray/fixed_sized_image_view.h"
#include "ash/common/system/tray/system_tray_controller.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_notification_view.h"
#include "ash/common/wm_shell.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::Notification;

namespace ash {
namespace {

display::DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

base::string16 GetDisplayName(int64_t display_id) {
  return base::UTF8ToUTF16(
      GetDisplayManager()->GetDisplayNameForId(display_id));
}

base::string16 GetDisplaySize(int64_t display_id) {
  display::DisplayManager* display_manager = GetDisplayManager();

  const display::Display* display =
      &display_manager->GetDisplayForId(display_id);

  // We don't show display size for mirrored display. Fallback
  // to empty string if this happens on release build.
  bool mirroring = display_manager->mirroring_display_id() == display_id;
  DCHECK(!mirroring);
  if (mirroring)
    return base::string16();

  DCHECK(display->is_valid());
  return base::UTF8ToUTF16(display->size().ToString());
}

// Attempts to open the display settings, returns true if successful.
bool OpenSettings() {
  // switch is intentionally introduced without default, to cause an error when
  // a new type of login status is introduced.
  switch (WmShell::Get()->system_tray_delegate()->GetUserLoginStatus()) {
    case LoginStatus::NOT_LOGGED_IN:
    case LoginStatus::LOCKED:
      return false;

    case LoginStatus::USER:
    case LoginStatus::OWNER:
    case LoginStatus::GUEST:
    case LoginStatus::PUBLIC:
    case LoginStatus::SUPERVISED:
    case LoginStatus::KIOSK_APP:
    case LoginStatus::ARC_KIOSK_APP:
      SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
      if (delegate->ShouldShowSettings()) {
        WmShell::Get()->system_tray_controller()->ShowDisplaySettings();
        return true;
      }
      break;
  }

  return false;
}

// Callback to handle a user selecting the notification view.
void OpenSettingsFromNotification() {
  WmShell::Get()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DISPLAY_NOTIFICATION_SELECTED);
  if (OpenSettings()) {
    WmShell::Get()->RecordUserMetricsAction(
        UMA_STATUS_AREA_DISPLAY_NOTIFICATION_SHOW_SETTINGS);
  }
}

// Returns the name of the currently connected external display. This should not
// be used when the external display is used for mirroring.
base::string16 GetExternalDisplayName() {
  display::DisplayManager* display_manager = GetDisplayManager();
  DCHECK(!display_manager->IsInMirrorMode());

  int64_t external_id = display::kInvalidDisplayId;
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    int64_t id = display_manager->GetDisplayAt(i).id();
    if (!display::Display::IsInternalDisplayId(id)) {
      external_id = id;
      break;
    }
  }

  if (external_id == display::kInvalidDisplayId)
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_UNKNOWN_DISPLAY_NAME);

  // The external display name may have an annotation of "(width x height)" in
  // case that the display is rotated or its resolution is changed.
  base::string16 name = GetDisplayName(external_id);
  const display::ManagedDisplayInfo& display_info =
      display_manager->GetDisplayInfo(external_id);
  if (display_info.GetActiveRotation() != display::Display::ROTATE_0 ||
      display_info.configured_ui_scale() != 1.0f ||
      !display_info.overscan_insets_in_dip().IsEmpty()) {
    name =
        l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATED_NAME,
                                   name, GetDisplaySize(external_id));
  } else if (display_info.overscan_insets_in_dip().IsEmpty() &&
             display_info.has_overscan()) {
    name = l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATED_NAME, name,
        l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_DISPLAY_ANNOTATION_OVERSCAN));
  }

  return name;
}

base::string16 GetDisplayMessage(base::string16* additional_message_out) {
  display::DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->GetNumDisplays() > 1) {
    if (display::Display::HasInternalDisplay()) {
      return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED,
                                        GetExternalDisplayName());
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL);
  }

  if (display_manager->IsInMirrorMode()) {
    if (display::Display::HasInternalDisplay()) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
          GetDisplayName(display_manager->mirroring_display_id()));
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING_NO_INTERNAL);
  }

  if (display_manager->IsInUnifiedMode())
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_UNIFIED);

  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  if (display::Display::HasInternalDisplay() &&
      !(display::Display::IsInternalDisplayId(primary_id))) {
    if (additional_message_out) {
      *additional_message_out = ash::SubstituteChromeOSDeviceType(
          IDS_ASH_STATUS_TRAY_DISPLAY_DOCKED_DESCRIPTION);
    }
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_DOCKED);
  }

  return base::string16();
}

}  // namespace

const char ScreenLayoutObserver::kNotificationId[] =
    "chrome://settings/display";

ScreenLayoutObserver::ScreenLayoutObserver() {
  WmShell::Get()->AddDisplayObserver(this);
  UpdateDisplayInfo(NULL);
}

ScreenLayoutObserver::~ScreenLayoutObserver() {
  WmShell::Get()->RemoveDisplayObserver(this);
}

void ScreenLayoutObserver::UpdateDisplayInfo(
    ScreenLayoutObserver::DisplayInfoMap* old_info) {
  if (old_info)
    old_info->swap(display_info_);
  display_info_.clear();

  display::DisplayManager* display_manager = GetDisplayManager();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    int64_t id = display_manager->GetDisplayAt(i).id();
    display_info_[id] = display_manager->GetDisplayInfo(id);
  }
}

bool ScreenLayoutObserver::GetDisplayMessageForNotification(
    const ScreenLayoutObserver::DisplayInfoMap& old_info,
    base::string16* message_out,
    base::string16* additional_message_out) {
  // Display is added or removed. Use the same message as the one in
  // the system tray.
  if (display_info_.size() != old_info.size()) {
    *message_out = GetDisplayMessage(additional_message_out);
    return true;
  }

  for (DisplayInfoMap::const_iterator iter = display_info_.begin();
       iter != display_info_.end(); ++iter) {
    DisplayInfoMap::const_iterator old_iter = old_info.find(iter->first);
    // The display's number is same but different displays. This happens
    // for the transition between docked mode and mirrored display. Falls back
    // to GetDisplayMessage().
    if (old_iter == old_info.end()) {
      *message_out = GetDisplayMessage(additional_message_out);
      return true;
    }

    if (iter->second.configured_ui_scale() !=
        old_iter->second.configured_ui_scale()) {
      *additional_message_out = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
          GetDisplayName(iter->first), GetDisplaySize(iter->first));
      return true;
    }
    if (iter->second.GetActiveRotation() !=
        old_iter->second.GetActiveRotation()) {
      int rotation_text_id = 0;
      switch (iter->second.GetActiveRotation()) {
        case display::Display::ROTATE_0:
          rotation_text_id = IDS_ASH_STATUS_TRAY_DISPLAY_STANDARD_ORIENTATION;
          break;
        case display::Display::ROTATE_90:
          rotation_text_id = IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_90;
          break;
        case display::Display::ROTATE_180:
          rotation_text_id = IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_180;
          break;
        case display::Display::ROTATE_270:
          rotation_text_id = IDS_ASH_STATUS_TRAY_DISPLAY_ORIENTATION_270;
          break;
      }
      *additional_message_out = l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DISPLAY_ROTATED, GetDisplayName(iter->first),
          l10n_util::GetStringUTF16(rotation_text_id));
      return true;
    }
  }

  // Found nothing special
  return false;
}

void ScreenLayoutObserver::CreateOrUpdateNotification(
    const base::string16& message,
    const base::string16& additional_message) {
  // Always remove the notification to make sure the notification appears
  // as a popup in any situation.
  message_center::MessageCenter::Get()->RemoveNotification(kNotificationId,
                                                           false /* by_user */);

  if (message.empty() && additional_message.empty())
    return;

  // Don't display notifications for accelerometer triggered screen rotations.
  // See http://crbug.com/364949
  if (Shell::GetInstance()
          ->screen_orientation_controller()
          ->ignore_display_configuration_updates()) {
    return;
  }

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, message,
      additional_message, bundle.GetImageNamed(IDR_AURA_NOTIFICATION_DISPLAY),
      base::string16(),  // display_source
      GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 system_notifier::kNotifierDisplay),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickedDelegate(
          base::Bind(&OpenSettingsFromNotification))));

  WmShell::Get()->RecordUserMetricsAction(
      UMA_STATUS_AREA_DISPLAY_NOTIFICATION_CREATED);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void ScreenLayoutObserver::OnDisplayConfigurationChanged() {
  DisplayInfoMap old_info;
  UpdateDisplayInfo(&old_info);

  if (!show_notifications_for_testing)
    return;

  base::string16 message;
  base::string16 additional_message;
  if (GetDisplayMessageForNotification(old_info, &message, &additional_message))
    CreateOrUpdateNotification(message, additional_message);
}

}  // namespace ash
