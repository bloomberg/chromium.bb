// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "ash/accelerators/accelerator_table.h"
#include "ash/components/shortcut_viewer/keyboard_shortcut_item.h"
#include "ash/components/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/views/accelerator_table.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string AcceleratorIdToString(
    const keyboard_shortcut_viewer::AcceleratorId& accelerator_id) {
  return base::StringPrintf(
      "keycode=%d shift=%s control=%s alt=%s search=%s", accelerator_id.keycode,
      (accelerator_id.modifiers & ui::EF_SHIFT_DOWN) ? "true" : "false",
      (accelerator_id.modifiers & ui::EF_CONTROL_DOWN) ? "true" : "false",
      (accelerator_id.modifiers & ui::EF_ALT_DOWN) ? "true" : "false",
      (accelerator_id.modifiers & ui::EF_COMMAND_DOWN) ? "true" : "false");
}

std::string AcceleratorIdsToString(
    const std::vector<keyboard_shortcut_viewer::AcceleratorId>&
        accelerator_ids) {
  std::vector<std::string> msgs;
  for (const auto& id : accelerator_ids)
    msgs.emplace_back(AcceleratorIdToString(id));
  return base::JoinString(msgs, ", ");
}

class KeyboardShortcutViewerMetadataTest : public testing::Test {
 public:
  KeyboardShortcutViewerMetadataTest() = default;
  ~KeyboardShortcutViewerMetadataTest() override = default;

  void SetUp() override {
    for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
      const ash::AcceleratorData& accel_data = ash::kAcceleratorData[i];
      ash_accelerator_ids_.insert({accel_data.keycode, accel_data.modifiers});
    }

    for (const auto& accel_mapping : GetAcceleratorList()) {
      chrome_accelerator_ids_.insert(
          {accel_mapping.keycode, accel_mapping.modifiers});
    }

    testing::Test::SetUp();
  }

 protected:
  // Ash accelerator ids.
  std::set<keyboard_shortcut_viewer::AcceleratorId> ash_accelerator_ids_;

  // Chrome accelerator ids.
  std::set<keyboard_shortcut_viewer::AcceleratorId> chrome_accelerator_ids_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardShortcutViewerMetadataTest);
};

}  // namespace

// Test that AcceleratorId has at least one corresponding accelerators in ash or
// chrome. Some |accelerator_id| might exist in both accelerator_tables, such as
// new window and new tab.
TEST_F(KeyboardShortcutViewerMetadataTest, CheckAcceleratorIdHasAccelerator) {
  for (const auto& shortcut_item :
       keyboard_shortcut_viewer::GetKeyboardShortcutItemList()) {
    for (const auto& accelerator_id : shortcut_item.accelerator_ids) {
      int number_of_accelerator = ash_accelerator_ids_.count(accelerator_id) +
                                  chrome_accelerator_ids_.count(accelerator_id);
      EXPECT_GE(number_of_accelerator, 1)
          << "accelerator_id has no corresponding accelerator: "
          << AcceleratorIdToString(accelerator_id);
    }
  }
}

// Test that AcceleratorIds have no duplicates.
TEST_F(KeyboardShortcutViewerMetadataTest, CheckAcceleratorIdsNoDuplicates) {
  std::set<keyboard_shortcut_viewer::AcceleratorId> accelerator_ids;
  for (const auto& shortcut_item :
       keyboard_shortcut_viewer::GetKeyboardShortcutItemList()) {
    for (const auto& accelerator_id : shortcut_item.accelerator_ids) {
      EXPECT_TRUE(accelerator_ids.insert(accelerator_id).second)
          << "Has duplicated accelerator_id: "
          << AcceleratorIdToString(accelerator_id);
    }
  }
}

// Test that metadata with empty |shortcut_key_codes| and grouped AcceleratorIds
// should have the same modifiers.
TEST_F(KeyboardShortcutViewerMetadataTest,
       CheckModifiersEqualForGroupedAcceleratorIdsWithEmptyShortcutKeyCodes) {
  for (const auto& shortcut_item :
       keyboard_shortcut_viewer::GetKeyboardShortcutItemList()) {
    // This test only checks metadata with empty |shortcut_key_codes| and
    // grouped |accelerator_ids|.
    if (!shortcut_item.shortcut_key_codes.empty() ||
        shortcut_item.accelerator_ids.size() <= 1)
      continue;

    const int modifiers = shortcut_item.accelerator_ids[0].modifiers;
    for (const auto& accelerator_id : shortcut_item.accelerator_ids) {
      EXPECT_EQ(modifiers, accelerator_id.modifiers)
          << "Grouped accelerator_ids with empty shortcut_key_codes should "
             "have same modifiers: "
          << AcceleratorIdsToString(shortcut_item.accelerator_ids);
    }
  }
}
