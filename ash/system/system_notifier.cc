// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/system_notifier.h"

#include "base/logging.h"

#if defined(OS_CHROMEOS)
#include "ui/chromeos/network/network_state_notifier.h"
#endif

namespace ash {
namespace system_notifier {

namespace {

// See http://dev.chromium.org/chromium-os/chromiumos-design-docs/
// system-notifications for the reasoning.

// |kAlwaysShownSystemNotifierIds| is the list of system notification sources
// which can appear regardless of the situation, such like login screen or lock
// screen.
const char* kAlwaysShownSystemNotifierIds[] = {
    kNotifierDeprecatedAccelerator,
    kNotifierBattery,
    kNotifierDisplay,
    kNotifierDisplayError,
#if defined(OS_CHROMEOS)
    ui::NetworkStateNotifier::kNotifierNetworkError,
#endif
    kNotifierPower,
    // Note: Order doesn't matter here, so keep this in alphabetic order, don't
    // just add your stuff at the end!
    NULL};

// |kAshSystemNotifiers| is the list of normal system notification sources for
// ash events. These notifications can be hidden in some context.
const char* kAshSystemNotifiers[] = {
  kNotifierBluetooth,
  kNotifierDisplayResolutionChange,
  kNotifierLocale,
  kNotifierMultiProfileFirstRun,
#if defined(OS_CHROMEOS)
  ui::NetworkStateNotifier::kNotifierNetwork,
#endif
  kNotifierNetworkPortalDetector,
  kNotifierScreenshot,
  kNotifierScreenCapture,
  kNotifierScreenShare,
  kNotifierSessionLengthTimeout,
  kNotifierSupervisedUser,
  kNotifierWebUsb,
  // Note: Order doesn't matter here, so keep this in alphabetic order, don't
  // just add your stuff at the end!
  NULL
};

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

const char kNotifierBattery[] = "ash.battery";
const char kNotifierBluetooth[] = "ash.bluetooth";
const char kNotifierDeprecatedAccelerator[] = "ash.accelerator-controller";
const char kNotifierDisplay[] = "ash.display";
const char kNotifierDisplayError[] = "ash.display.error";
const char kNotifierDisplayResolutionChange[] = "ash.display.resolution-change";
const char kNotifierDualRole[] = "ash.dual-role";
const char kNotifierLocale[] = "ash.locale";
const char kNotifierMultiProfileFirstRun[] = "ash.multi-profile.first-run";
const char kNotifierNetworkPortalDetector[] = "ash.network.portal-detector";
const char kNotifierPower[] = "ash.power";
const char kNotifierScreenshot[] = "ash.screenshot";
const char kNotifierScreenCapture[] = "ash.screen-capture";
const char kNotifierScreenShare[] = "ash.screen-share";
const char kNotifierSessionLengthTimeout[] = "ash.session-length-timeout";
const char kNotifierSupervisedUser[] = "ash.locally-managed-user";
const char kNotifierWebUsb[] = "ash.webusb";

bool ShouldAlwaysShowPopups(const message_center::NotifierId& notifier_id) {
  return MatchSystemNotifierId(notifier_id, kAlwaysShownSystemNotifierIds);
}

bool IsAshSystemNotifier(const message_center::NotifierId& notifier_id) {
  return ShouldAlwaysShowPopups(notifier_id) ||
         MatchSystemNotifierId(notifier_id, kAshSystemNotifiers);
}

}  // namespace system_notifier
}  // namespace ash
