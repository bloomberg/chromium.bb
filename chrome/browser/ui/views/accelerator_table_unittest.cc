// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>

#include "build/build_config.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"

#if defined(USE_ASH)
#include "ash/common/accelerators/accelerator_table.h"
#endif  // USE_ASH

namespace chrome {

namespace {

struct Cmp {
  bool operator()(const AcceleratorMapping& lhs,
                  const AcceleratorMapping& rhs) const {
    if (lhs.keycode != rhs.keycode)
      return lhs.keycode < rhs.keycode;
    return lhs.modifiers < rhs.modifiers;
    // Do not check |command_id|.
  }
};

}  // namespace

TEST(AcceleratorTableTest, CheckDuplicatedAccelerators) {
  std::set<AcceleratorMapping, Cmp> accelerators;
  const std::vector<AcceleratorMapping> accelerator_list(GetAcceleratorList());
  for (std::vector<AcceleratorMapping>::const_iterator it =
           accelerator_list.begin(); it != accelerator_list.end(); ++it) {
    const AcceleratorMapping& entry = *it;
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.keycode << ", "
        << (entry.modifiers & ui::EF_SHIFT_DOWN) << ", "
        << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN);
  }
}

#if defined(OS_CHROMEOS)
TEST(AcceleratorTableTest, CheckDuplicatedAcceleratorsAsh) {
  std::set<AcceleratorMapping, Cmp> accelerators;
  const std::vector<AcceleratorMapping> accelerator_list(GetAcceleratorList());
  for (std::vector<AcceleratorMapping>::const_iterator it =
           accelerator_list.begin(); it != accelerator_list.end(); ++it) {
    const AcceleratorMapping& entry = *it;
    accelerators.insert(entry);
  }
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    const ash::AcceleratorData& ash_entry = ash::kAcceleratorData[i];
    if (!ash_entry.trigger_on_press)
      continue;  // kAcceleratorMap does not have any release accelerators.
    // The shortcuts to toggle minimized state, to show the task manager,
    // to toggle touch HUD, and to open help page are defined on browser side
    // as well as ash side by design so that web contents can consume these
    // short cuts. (see crbug.com/309915, 370019, 412435, 321568 and CL)
    if (ash_entry.action == ash::WINDOW_MINIMIZE ||
        ash_entry.action == ash::SHOW_TASK_MANAGER ||
        ash_entry.action == ash::TOUCH_HUD_PROJECTION_TOGGLE ||
        ash_entry.action == ash::OPEN_GET_HELP)
      continue;
    AcceleratorMapping entry;
    entry.keycode = ash_entry.keycode;
    entry.modifiers = ash_entry.modifiers;
    entry.command_id = 0;  // dummy
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.keycode << ", "
        << (entry.modifiers & ui::EF_SHIFT_DOWN) << ", "
        << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN);
  }
}
#endif  // USE_ASH

}  // namespace chrome
