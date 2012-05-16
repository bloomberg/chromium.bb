// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/basictypes.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/events.h"

#if defined(USE_ASH)
#include "ash/accelerators/accelerator_table.h"
#endif  // USE_ASH

namespace {

struct Cmp {
  bool operator()(const browser::AcceleratorMapping& lhs,
                  const browser::AcceleratorMapping& rhs) {
    if (lhs.keycode != rhs.keycode)
      return lhs.keycode < rhs.keycode;
    return lhs.modifiers < rhs.modifiers;
    // Do not check |command_id|.
  }
};

}  // namespace

TEST(AcceleratorTableTest, CheckDuplicatedAccelerators) {
  std::set<browser::AcceleratorMapping, Cmp> acclerators;
  for (size_t i = 0; i < browser::kAcceleratorMapLength; ++i) {
    const browser::AcceleratorMapping& entry = browser::kAcceleratorMap[i];
    EXPECT_TRUE(acclerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.keycode << ", "
        << (entry.modifiers & ui::EF_SHIFT_DOWN) << ", "
        << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN);
  }
}

#if defined(USE_ASH)
TEST(AcceleratorTableTest, CheckDuplicatedAcceleratorsAsh) {
  std::set<browser::AcceleratorMapping, Cmp> acclerators;
  for (size_t i = 0; i < browser::kAcceleratorMapLength; ++i) {
    const browser::AcceleratorMapping& entry = browser::kAcceleratorMap[i];
    acclerators.insert(entry);
  }
  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    const ash::AcceleratorData& ash_entry = ash::kAcceleratorData[i];
    if (!ash_entry.trigger_on_press)
      continue;  // kAcceleratorMap does not have any release accelerators.
    browser::AcceleratorMapping entry;
    entry.keycode = ash_entry.keycode;
    entry.modifiers = ash_entry.modifiers;
    entry.command_id = 0;  // dummy
    EXPECT_TRUE(acclerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.keycode << ", "
        << (entry.modifiers & ui::EF_SHIFT_DOWN) << ", "
        << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN);
  }
}
#endif  // USE_ASH
