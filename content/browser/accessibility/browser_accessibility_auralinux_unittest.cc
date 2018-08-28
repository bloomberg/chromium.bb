// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_auralinux.h"

#include <atk/atk.h>
#include <memory>
#include <string>
#include <vector>

#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_node_auralinux.h"

namespace content {

class BrowserAccessibilityAuraLinuxTest : public testing::Test {
 public:
  BrowserAccessibilityAuraLinuxTest();
  ~BrowserAccessibilityAuraLinuxTest() override;

 private:
  void SetUp() override;

  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityAuraLinuxTest);
};

BrowserAccessibilityAuraLinuxTest::BrowserAccessibilityAuraLinuxTest() {}

BrowserAccessibilityAuraLinuxTest::~BrowserAccessibilityAuraLinuxTest() {}

void BrowserAccessibilityAuraLinuxTest::SetUp() {}

TEST_F(BrowserAccessibilityAuraLinuxTest, TestSimpleAtkText) {
  ui::AXNodeData root_data;
  root_data.id = 1;
  root_data.role = ax::mojom::Role::kStaticText;
  root_data.SetName("\xE2\x98\xBA Multiple Words");

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(MakeAXTreeUpdate(root_data), nullptr,
                                          new BrowserAccessibilityFactory()));

  ui::AXPlatformNodeAuraLinux* root_obj =
      ToBrowserAccessibilityAuraLinux(manager->GetRoot())->GetNode();
  AtkObject* root_atk_object(root_obj->GetNativeViewAccessible());
  ASSERT_TRUE(ATK_IS_OBJECT(root_atk_object));
  ASSERT_TRUE(ATK_IS_TEXT(root_atk_object));
  g_object_ref(root_atk_object);

  AtkText* atk_text = ATK_TEXT(root_atk_object);

  auto verify_atk_text_contents = [&](const char* expected_text,
                                      int start_offset, int end_offset) {
    gchar* text = atk_text_get_text(atk_text, start_offset, end_offset);
    EXPECT_STREQ(expected_text, text);
    g_free(text);
  };

  verify_atk_text_contents("\xE2\x98\xBA Multiple Words", 0, -1);
  verify_atk_text_contents("Multiple Words", 2, -1);
  verify_atk_text_contents("\xE2\x98\xBA", 0, 1);

  EXPECT_EQ(16, atk_text_get_character_count(atk_text));

  g_object_unref(root_atk_object);

  manager.reset();
}

TEST_F(BrowserAccessibilityAuraLinuxTest, TestCompositeAtkText) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";
  const int text_name_len = text1_name.length() + text2_name.length();

  ui::AXNodeData text1;
  text1.id = 11;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName(text1_name);

  ui::AXNodeData text2;
  text2.id = 12;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName(text2_name);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(text1.id);
  root.child_ids.push_back(text2.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(MakeAXTreeUpdate(root, text1, text2),
                                          nullptr,
                                          new BrowserAccessibilityFactory()));

  ui::AXPlatformNodeAuraLinux* root_obj =
      ToBrowserAccessibilityAuraLinux(manager->GetRoot())->GetNode();
  AtkObject* root_atk_object(root_obj->GetNativeViewAccessible());

  ASSERT_TRUE(ATK_IS_OBJECT(root_atk_object));
  ASSERT_TRUE(ATK_IS_TEXT(root_atk_object));
  g_object_ref(root_atk_object);
  AtkText* atk_text = ATK_TEXT(root_atk_object);

  EXPECT_EQ(text_name_len, atk_text_get_character_count(atk_text));

  gchar* text = atk_text_get_text(atk_text, 0, -1);
  EXPECT_STREQ((text1_name + text2_name).c_str(), text);
  g_free(text);

  g_object_unref(root_atk_object);

  manager.reset();
}

}  // namespace content
