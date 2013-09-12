// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SYSTEM_NOTIFIER_H_
#define ASH_SYSTEM_SYSTEM_NOTIFIER_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/message_center/notifier_settings.h"

namespace ash {
namespace system_notifier {

enum AshSystemComponentNotifierType {
  NOTIFIER_NO_SYSTEM_COMPONENT = -1,

  // Alphabetical order.
  NOTIFIER_DISPLAY,
  NOTIFIER_DISPLAY_RESOLUTION_CHANGE,
  NOTIFIER_DISPLAY_ERROR,
  NOTIFIER_INPUT_METHOD,
  NOTIFIER_LOCALE,
  NOTIFIER_LOCALLY_MANAGED_USER,
  NOTIFIER_NETWORK,
  NOTIFIER_NETWORK_ERROR,
  NOTIFIER_SCREENSHOT,
  NOTIFIER_SCREEN_CAPTURE,
  NOTIFIER_SCREEN_SHARE,
  NOTIFIER_SESSION_LENGTH_TIMEOUT,
  NOTIFIER_POWER,
};

ASH_EXPORT std::string SystemComponentTypeToString(
    AshSystemComponentNotifierType type);

// Returns true if notifications from |notifier_id| should always appear as
// popups. "Always appear" means the popups should appear even in login screen,
// lock screen, or fullscreen state.
ASH_EXPORT bool ShouldAlwaysShowPopups(
    const message_center::NotifierId& notifier_id);

}  // namespace system_notifier
}  // namespace ash

#endif  // ASH_SYSTEM_SYSTEM_NOTIFIER_H_
