// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/system_notifier.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/message_center/public/cpp/message_center_switches.h"

namespace ash {
namespace system_notifier {

namespace {

// See http://dev.chromium.org/chromium-os/chromiumos-design-docs/
// system-notifications for the reasoning.

// |kAlwaysShownSystemNotifierIds| is the list of system notification sources
// which can appear regardless of the situation, such like login screen or lock
// screen.
const char* kAlwaysShownSystemNotifierIds[] = {
    kNotifierAccessibility, kNotifierDeprecatedAccelerator, kNotifierBattery,
    kNotifierDisplay, kNotifierDisplayError, kNotifierNetworkError,
    kNotifierPower, kNotifierStylusBattery,
    // Note: Order doesn't matter here, so keep this in alphabetic order, don't
    // just add your stuff at the end!
    NULL};

// |kAshSystemNotifiers| is the list of normal system notification sources for
// ash events. These notifications can be hidden in some context.
const char* kAshSystemNotifiers[] = {
    kNotifierBluetooth, kNotifierCapsLock, kNotifierDisplayResolutionChange,
    kNotifierDisk, kNotifierLocale, kNotifierMultiProfileFirstRun,
    kNotifierNetwork, kNotifierNetworkPortalDetector, kNotifierScreenshot,
    kNotifierScreenCapture, kNotifierScreenShare, kNotifierSessionLengthTimeout,
    kNotifierSms, kNotifierSupervisedUser, kNotifierTether, kNotifierWebUsb,
    kNotifierWifiToggle,
    // Note: Order doesn't matter here, so keep this in alphabetic order, don't
    // just add your stuff at the end!
    NULL};

bool MatchSystemNotifierId(const message_center::NotifierId& notifier_id,
                           const char* id_list[]) {
  if (notifier_id.type != message_center::NotifierId::SYSTEM_COMPONENT)
    return false;

  for (size_t i = 0; id_list[i] != NULL; ++i) {
    if (notifier_id.id == id_list[i])
      return true;
  }
  return false;
}

}  // namespace

const char kNotifierAccessibility[] = "ash.accessibility";
const char kNotifierBattery[] = "ash.battery";
const char kNotifierBluetooth[] = "ash.bluetooth";
const char kNotifierCapsLock[] = "ash.caps-lock";
const char kNotifierDeprecatedAccelerator[] = "ash.accelerator-controller";
const char kNotifierDisk[] = "ash.disk";
const char kNotifierDisplay[] = "ash.display";
const char kNotifierDisplayError[] = "ash.display.error";
const char kNotifierDisplayResolutionChange[] = "ash.display.resolution-change";
const char kNotifierDualRole[] = "ash.dual-role";
const char kNotifierFingerprintUnlock[] = "ash.fingerprintunlock";
const char kNotifierHats[] = "ash.hats";
const char kNotifierLocale[] = "ash.locale";
const char kNotifierMultiProfileFirstRun[] = "ash.multi-profile.first-run";
const char kNotifierNetwork[] = "ash.network";
const char kNotifierNetworkError[] = "ash.network.error";
const char kNotifierNetworkPortalDetector[] = "ash.network.portal-detector";
const char kNotifierPinUnlock[] = "ash.pinunlock";
const char kNotifierPower[] = "ash.power";
const char kNotifierScreenshot[] = "ash.screenshot";
const char kNotifierScreenCapture[] = "ash.screen-capture";
const char kNotifierScreenShare[] = "ash.screen-share";
const char kNotifierSessionLengthTimeout[] = "ash.session-length-timeout";
const char kNotifierSms[] = "ash.sms";
const char kNotifierStylusBattery[] = "ash.stylus-battery";
const char kNotifierSupervisedUser[] = "ash.locally-managed-user";
const char kNotifierTether[] = "ash.tether";
const char kNotifierWebUsb[] = "ash.webusb";
const char kNotifierWifiToggle[] = "ash.wifi-toggle";

bool ShouldAlwaysShowPopups(const message_center::NotifierId& notifier_id) {
  return MatchSystemNotifierId(notifier_id, kAlwaysShownSystemNotifierIds);
}

bool IsAshSystemNotifier(const message_center::NotifierId& notifier_id) {
  return ShouldAlwaysShowPopups(notifier_id) ||
         MatchSystemNotifierId(notifier_id, kAshSystemNotifiers);
}

std::unique_ptr<message_center::Notification> CreateSystemNotification(
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
    message_center::SystemNotificationWarningLevel color_type) {
  if (message_center::IsNewStyleNotificationEnabled()) {
    return message_center::Notification::CreateSystemNotification(
        type, id, title, message, gfx::Image(), display_source, origin_url,
        notifier_id, optional_fields, delegate, small_image, color_type);
  }
  return base::MakeUnique<message_center::Notification>(
      type, id, title, message, icon, display_source, origin_url, notifier_id,
      optional_fields, delegate);
}

}  // namespace system_notifier
}  // namespace ash
