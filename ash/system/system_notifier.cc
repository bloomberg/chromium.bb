// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/system_notifier.h"

#include "base/logging.h"

namespace ash {
namespace system_notifier {

namespace {

// See http://dev.chromium.org/chromium-os/chromiumos-design-docs/
// system-notifications for the reasoning.
const char* kAlwaysShownNotifierIds[] = {
  kNotifierDisplay,
  kNotifierDisplayError,
  kNotifierNetworkError,
  kNotifierPower,
  NULL
};

const char* kAshSystemNotifiers[] = {
  kNotifierAuthError,
  kNotifierDisplay,
  kNotifierDisplayResolutionChange,
  kNotifierDisplayError,
  kNotifierLocale,
  kNotifierLocallyManagedUser,
  kNotifierMultiProfileFirstRun,
  kNotifierNetwork,
  kNotifierNetworkError,
  kNotifierNetworkPortalDetector,
  kNotifierScreenshot,
  kNotifierScreenCapture,
  kNotifierScreenShare,
  kNotifierSessionLengthTimeout,
  kNotifierPower,
  kNotifierBluetooth,
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

const char kNotifierAuthError[] = "ash.auth.error";
const char kNotifierBluetooth[] = "ash.bluetooth";
const char kNotifierDisplay[] = "ash.display";
const char kNotifierDisplayResolutionChange[] = "ash.display.resolution-change";
const char kNotifierDisplayError[] = "ash.display.error";
const char kNotifierLocale[] = "ash.locale";
const char kNotifierLocallyManagedUser[] = "ash.locally-managed-user";
const char kNotifierMultiProfileFirstRun[] = "ash.multi-profile.first-run";
const char kNotifierNetwork[] = "ash.network";
const char kNotifierNetworkError[] = "ash.network.error";
const char kNotifierNetworkPortalDetector[] = "ash.network.portal-detector";
const char kNotifierPower[] = "ash.power";
const char kNotifierScreenshot[] = "ash.screenshot";
const char kNotifierScreenCapture[] = "ash.screen-capture";
const char kNotifierScreenShare[] = "ash.screen-share";
const char kNotifierSessionLengthTimeout[] = "ash.session-length-timeout";

bool ShouldAlwaysShowPopups(const message_center::NotifierId& notifier_id) {
  return MatchSystemNotifierId(notifier_id, kAlwaysShownNotifierIds);
}

bool IsAshSystemNotifier(const message_center::NotifierId& notifier_id) {
  return MatchSystemNotifierId(notifier_id, kAshSystemNotifiers);
}

}  // namespace system_notifier
}  // namespace ash
