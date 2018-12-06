// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_id_list.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(GamepadIdTest, NoDuplicateIds) {
  // Each vendor/product ID pair and each GamepadId should appear only once in
  // the table of known gamepads.
  const auto gamepads = GamepadIdList::Get().GetGamepadListForTesting();
  std::unordered_set<uint32_t> seen_vendor_product_ids;
  std::unordered_set<GamepadId> seen_gamepad_ids;
  for (const auto& item : gamepads) {
    uint32_t vendor_product_id = (std::get<0>(item) << 16) | std::get<1>(item);
    GamepadId gamepad_id = std::get<3>(item);
    seen_vendor_product_ids.insert(vendor_product_id);
    seen_gamepad_ids.insert(gamepad_id);
    EXPECT_LE(gamepad_id, GamepadId::kMaxValue);
  }
  EXPECT_EQ(seen_vendor_product_ids.size(), gamepads.size());
  EXPECT_EQ(seen_gamepad_ids.size(), gamepads.size());
  EXPECT_EQ(static_cast<size_t>(GamepadId::kMaxValue), gamepads.size());
}

}  // namespace device
