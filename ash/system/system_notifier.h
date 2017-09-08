// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SYSTEM_NOTIFIER_H_
#define ASH_SYSTEM_SYSTEM_NOTIFIER_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notifier_settings.h"

namespace ash {
namespace system_notifier {

// The list of ash system notifier IDs. Alphabetical order.
ASH_EXPORT extern const char kNotifierAccessibility[];
ASH_EXPORT extern const char kNotifierBattery[];
ASH_EXPORT extern const char kNotifierBluetooth[];
ASH_EXPORT extern const char kNotifierCapsLock[];
ASH_EXPORT extern const char kNotifierDeprecatedAccelerator[];
ASH_EXPORT extern const char kNotifierDisk[];
ASH_EXPORT extern const char kNotifierDisplay[];
ASH_EXPORT extern const char kNotifierDisplayResolutionChange[];
ASH_EXPORT extern const char kNotifierDisplayError[];
ASH_EXPORT extern const char kNotifierDualRole[];
ASH_EXPORT extern const char kNotifierFingerprintUnlock[];
ASH_EXPORT extern const char kNotifierHats[];
ASH_EXPORT extern const char kNotifierLocale[];
ASH_EXPORT extern const char kNotifierMultiProfileFirstRun[];
ASH_EXPORT extern const char kNotifierNetwork[];
ASH_EXPORT extern const char kNotifierNetworkError[];
ASH_EXPORT extern const char kNotifierNetworkPortalDetector[];
ASH_EXPORT extern const char kNotifierPinUnlock[];
ASH_EXPORT extern const char kNotifierPower[];
ASH_EXPORT extern const char kNotifierScreenshot[];
ASH_EXPORT extern const char kNotifierScreenCapture[];
ASH_EXPORT extern const char kNotifierScreenShare[];
ASH_EXPORT extern const char kNotifierSessionLengthTimeout[];
ASH_EXPORT extern const char kNotifierSms[];
ASH_EXPORT extern const char kNotifierStylusBattery[];
ASH_EXPORT extern const char kNotifierSupervisedUser[];
ASH_EXPORT extern const char kNotifierTether[];
ASH_EXPORT extern const char kNotifierWebUsb[];
ASH_EXPORT extern const char kNotifierWifiToggle[];

// Returns true if notifications from |notifier_id| should always appear as
// popups. "Always appear" means the popups should appear even in login screen,
// lock screen, or fullscreen state.
ASH_EXPORT bool ShouldAlwaysShowPopups(
    const message_center::NotifierId& notifier_id);

// Returns true if |notifier_id| is the system notifier from Ash.
ASH_EXPORT bool IsAshSystemNotifier(
    const message_center::NotifierId& notifier_id);

// Utility function to call Notification::CreateSystemNotification when
// IsNewStyleNotificationEnabled() is true, and otherwise call
// Notification constructor directly.
// When IsNewStyleNotificationEnabled() is true, |icon| will be ignored.
// When IsNewStyleNotificationEnabled() is false, |small_image| and |color_type|
// will be ignored.
// TODO(tetsui): Remove this function when new style notification becomes
// default.
ASH_EXPORT std::unique_ptr<message_center::Notification>
CreateSystemNotification(
    message_center::NotificationType type,
    const std::string& id,
    const base::string16& title,
    const base::string16& message,
    const gfx::Image& icon,
    const base::string16& display_source,
    const GURL& origin_url,
    const message_center::NotifierId& notifier_id,
    const message_center::RichNotificationData& optional_fields,
    scoped_refptr<message_center::NotificationDelegate> delegate,
    const gfx::VectorIcon& small_image,
    message_center::SystemNotificationWarningLevel color_type);

}  // namespace system_notifier
}  // namespace ash

#endif  // ASH_SYSTEM_SYSTEM_NOTIFIER_H_
