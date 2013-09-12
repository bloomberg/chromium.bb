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
const AshSystemComponentNotifierType kAlwaysShownNotifierIds[] = {
  NOTIFIER_DISPLAY,
  NOTIFIER_DISPLAY_ERROR,
  NOTIFIER_POWER,
};

}  // namespace

std::string SystemComponentTypeToString(AshSystemComponentNotifierType type) {
  if (type == NOTIFIER_SCREENSHOT)
    return "screenshot";

  // TODO(mukai): fill the names of other components.
  NOTIMPLEMENTED();
  return std::string();
}

bool ShouldAlwaysShowPopups(const message_center::NotifierId& notifier_id) {
  if (notifier_id.type != message_center::NotifierId::SYSTEM_COMPONENT)
    return false;

  for (size_t i = 0; i < arraysize(kAlwaysShownNotifierIds); ++i) {
    if (notifier_id.system_component_type == kAlwaysShownNotifierIds[i])
      return true;
  }
  return false;
}

}  // namespace system_notifier
}  // namespace ash
