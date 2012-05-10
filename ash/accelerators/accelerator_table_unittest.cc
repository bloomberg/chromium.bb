// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/basictypes.h"
#include "ash/accelerators/accelerator_table.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

struct Cmp {
  bool operator()(const AcceleratorData& lhs,
                  const AcceleratorData& rhs) {
    if (lhs.trigger_on_press != rhs.trigger_on_press)
      return lhs.trigger_on_press < rhs.trigger_on_press;
    if (lhs.keycode != rhs.keycode)
      return lhs.keycode < rhs.keycode;
    if (lhs.shift != rhs.shift)
      return lhs.shift < rhs.shift;
    if (lhs.ctrl != rhs.ctrl)
      return lhs.ctrl < rhs.ctrl;
    return lhs.alt < rhs.alt;
    // Do not check |action|.
  }
};

}  // namespace

TEST(AcceleratorTableTest, CheckDuplicatedAccelerators) {
  std::set<AcceleratorData, Cmp> acclerators;
  for (size_t i = 0; i < kAcceleratorDataLength; ++i) {
    const AcceleratorData& entry = kAcceleratorData[i];
    EXPECT_TRUE(acclerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.trigger_on_press << ", "
        << entry.keycode << ", " << entry.shift << ", " << entry.ctrl << ", "
        << entry.alt;
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedActionsAllowedAtLoginScreen) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kActionsAllowedAtLoginScreenLength; ++i) {
    EXPECT_TRUE(actions.insert(kActionsAllowedAtLoginScreen[i]).second)
        << "Duplicated action: " << kActionsAllowedAtLoginScreen[i];
  }
}

}  // namespace ash
