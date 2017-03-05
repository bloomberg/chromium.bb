// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "ui/base/accelerators/accelerator.h"

#if defined(USE_ASH)
#include "ash/common/accelerators/accelerator_table.h"  // nogncheck
#endif  // USE_ASH

namespace chrome {

bool IsChromeAccelerator(const ui::Accelerator& accelerator, Profile* profile) {
#if defined(USE_ASH)
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    const ash::AcceleratorData& accel_data = ash::kAcceleratorData[i];
    if (accel_data.keycode == accelerator.key_code() &&
        accel_data.modifiers == accelerator.modifiers()) {
      return true;
    }
  }
#endif

  const std::vector<AcceleratorMapping> accelerators = GetAcceleratorList();
  for (const auto& entry : accelerators) {
    if (entry.keycode == accelerator.key_code() &&
        entry.modifiers == accelerator.modifiers())
      return true;
  }

  return false;
}

ui::Accelerator GetPrimaryChromeAcceleratorForCommandId(int command_id) {
  ui::Accelerator accelerator;
  // GetAshAcceleratorForCommandId with HOST_DESKTOP_TYPE_ASH is used so we can
  // find Ash accelerators if Ash is supported on this platform, even if it's
  // not currently in use.
  if (GetStandardAcceleratorForCommandId(command_id, &accelerator) ||
      GetAshAcceleratorForCommandId(command_id, &accelerator)) {
    return accelerator;
  }

  std::vector<AcceleratorMapping> accelerators = GetAcceleratorList();
  for (size_t i = 0; i < accelerators.size(); ++i) {
    if (accelerators[i].command_id == command_id) {
      return ui::Accelerator(accelerators[i].keycode,
                             accelerators[i].modifiers);
    }
  }

  return ui::Accelerator();
}

}  // namespace chrome
