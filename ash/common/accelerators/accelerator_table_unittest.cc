// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "ash/common/accelerators/accelerator_table.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

struct Cmp {
  bool operator()(const AcceleratorData& lhs, const AcceleratorData& rhs) {
    if (lhs.trigger_on_press != rhs.trigger_on_press)
      return lhs.trigger_on_press < rhs.trigger_on_press;
    if (lhs.keycode != rhs.keycode)
      return lhs.keycode < rhs.keycode;
    return lhs.modifiers < rhs.modifiers;
    // Do not check |action|.
  }
};

}  // namespace

TEST(AcceleratorTableTest, CheckDuplicatedAccelerators) {
  std::set<AcceleratorData, Cmp> accelerators;
  for (size_t i = 0; i < kAcceleratorDataLength; ++i) {
    const AcceleratorData& entry = kAcceleratorData[i];
    EXPECT_TRUE(accelerators.insert(entry).second)
        << "Duplicated accelerator: " << entry.trigger_on_press << ", "
        << entry.keycode << ", " << (entry.modifiers & ui::EF_SHIFT_DOWN)
        << ", " << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN);
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedReservedActions) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kReservedActionsLength; ++i) {
    EXPECT_TRUE(actions.insert(kReservedActions[i]).second)
        << "Duplicated action: " << kReservedActions[i];
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedActionsAllowedAtLoginOrLockScreen) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kActionsAllowedAtLoginOrLockScreenLength; ++i) {
    EXPECT_TRUE(actions.insert(kActionsAllowedAtLoginOrLockScreen[i]).second)
        << "Duplicated action: " << kActionsAllowedAtLoginOrLockScreen[i];
  }
  for (size_t i = 0; i < kActionsAllowedAtLockScreenLength; ++i) {
    EXPECT_TRUE(actions.insert(kActionsAllowedAtLockScreen[i]).second)
        << "Duplicated action: " << kActionsAllowedAtLockScreen[i];
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedActionsAllowedAtModalWindow) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kActionsAllowedAtModalWindowLength; ++i) {
    EXPECT_TRUE(actions.insert(kActionsAllowedAtModalWindow[i]).second)
        << "Duplicated action: " << kActionsAllowedAtModalWindow[i]
        << " at index: " << i;
  }
}

TEST(AcceleratorTableTest, CheckDuplicatedRepeatableActions) {
  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kRepeatableActionsLength; ++i) {
    EXPECT_TRUE(actions.insert(kRepeatableActions[i]).second)
        << "Duplicated action: " << kRepeatableActions[i] << " at index: " << i;
  }
}

TEST(AcceleratorTableTest, CheckDeprecatedAccelerators) {
  std::set<AcceleratorData, Cmp> deprecated_actions;
  for (size_t i = 0; i < kDeprecatedAcceleratorsLength; ++i) {
    // A deprecated action can never appear twice in the list.
    const AcceleratorData& entry = kDeprecatedAccelerators[i];
    EXPECT_TRUE(deprecated_actions.insert(entry).second)
        << "Duplicate deprecated accelerator: " << entry.trigger_on_press
        << ", " << entry.keycode << ", "
        << (entry.modifiers & ui::EF_SHIFT_DOWN) << ", "
        << (entry.modifiers & ui::EF_CONTROL_DOWN) << ", "
        << (entry.modifiers & ui::EF_ALT_DOWN);
  }

  std::set<AcceleratorAction> actions;
  for (size_t i = 0; i < kDeprecatedAcceleratorsDataLength; ++i) {
    // There must never be any duplicated actions.
    const DeprecatedAcceleratorData& data = kDeprecatedAcceleratorsData[i];
    EXPECT_TRUE(actions.insert(data.action).second) << "Deprecated action: "
                                                    << data.action;

    // The UMA histogram name must be of the format "Ash.Accelerators.*"
    std::string uma_histogram(data.uma_histogram_name);
    EXPECT_TRUE(base::StartsWith(uma_histogram, "Ash.Accelerators.",
                                 base::CompareCase::SENSITIVE));
  }
}

}  // namespace ash
