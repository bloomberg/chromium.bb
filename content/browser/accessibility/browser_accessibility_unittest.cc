// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BrowserAccessibilityTest : public testing::Test {
 public:
  BrowserAccessibilityTest();
  ~BrowserAccessibilityTest() override;

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityTest);
};

BrowserAccessibilityTest::BrowserAccessibilityTest() {}

BrowserAccessibilityTest::~BrowserAccessibilityTest() {}

TEST_F(BrowserAccessibilityTest, TestCanFireEvents) {
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName("One two three.");

  ui::AXNodeData para1;
  para1.id = 11;
  para1.role = ax::mojom::Role::kParagraph;
  para1.child_ids.push_back(text1.id);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(para1.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(MakeAXTreeUpdate(root, para1, text1),
                                          nullptr,
                                          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_obj = manager->GetRoot();
  EXPECT_FALSE(root_obj->PlatformIsLeaf());
  EXPECT_TRUE(root_obj->CanFireEvents());

  BrowserAccessibility* para_obj = root_obj->PlatformGetChild(0);
  EXPECT_TRUE(para_obj->CanFireEvents());
#if defined(OS_ANDROID)
  EXPECT_TRUE(para_obj->PlatformIsLeaf());
#else
  EXPECT_FALSE(para_obj->PlatformIsLeaf());
#endif

  BrowserAccessibility* text_obj = manager->GetFromID(111);
  EXPECT_TRUE(text_obj->PlatformIsLeaf());
  EXPECT_TRUE(text_obj->CanFireEvents());

  manager.reset();
}

}  // namespace content
