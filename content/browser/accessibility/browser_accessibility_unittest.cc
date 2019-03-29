// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

#define EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants,                 \
                                                expected_descendants)        \
  {                                                                          \
    size_t count = descendants.size();                                       \
    EXPECT_EQ(count, expected_descendants.size());                           \
    for (size_t i = 0; i < count; ++i) {                                     \
      EXPECT_EQ(ui::AXPlatformNode::FromNativeViewAccessible(descendants[i]) \
                    ->GetDelegate()                                          \
                    ->GetData()                                              \
                    .ToString(),                                             \
                ui::AXPlatformNode::FromNativeViewAccessible(                \
                    expected_descendants[i])                                 \
                    ->GetDelegate()                                          \
                    ->GetData()                                              \
                    .ToString());                                            \
    }                                                                        \
  }

class BrowserAccessibilityTest : public testing::Test {
 public:
  BrowserAccessibilityTest();
  ~BrowserAccessibilityTest() override;

 protected:
  std::unique_ptr<TestBrowserAccessibilityDelegate>
      test_browser_accessibility_delegate_;

 private:
  void SetUp() override;

  base::test::ScopedTaskEnvironment task_environment_;
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityTest);
};

BrowserAccessibilityTest::BrowserAccessibilityTest() {}

BrowserAccessibilityTest::~BrowserAccessibilityTest() {}

void BrowserAccessibilityTest::SetUp() {
  test_browser_accessibility_delegate_ =
      std::make_unique<TestBrowserAccessibilityDelegate>();
}

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
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, para1, text1),
          test_browser_accessibility_delegate_.get(),
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

#if defined(OS_WIN) || BUILDFLAG(USE_ATK)
TEST_F(BrowserAccessibilityTest, TestGetDescendants) {
  // Set up ax tree with the following structure:
  //
  // root____________
  // |               |
  // para1___        text3
  // |       |
  // text1   text2
  ui::AXNodeData text1;
  text1.id = 111;
  text1.role = ax::mojom::Role::kStaticText;
  text1.SetName("One two three.");

  ui::AXNodeData text2;
  text2.id = 112;
  text2.role = ax::mojom::Role::kStaticText;
  text2.SetName("Two three four.");

  ui::AXNodeData text3;
  text3.id = 113;
  text3.role = ax::mojom::Role::kStaticText;
  text3.SetName("Three four five.");

  ui::AXNodeData para1;
  para1.id = 11;
  para1.role = ax::mojom::Role::kParagraph;
  para1.child_ids.push_back(text1.id);
  para1.child_ids.push_back(text2.id);

  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.child_ids.push_back(para1.id);
  root.child_ids.push_back(text3.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, para1, text1, text2, text3),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_obj = manager->GetRoot();
  BrowserAccessibility* para_obj = root_obj->PlatformGetChild(0);
  BrowserAccessibility* text1_obj = manager->GetFromID(111);
  BrowserAccessibility* text2_obj = manager->GetFromID(112);
  BrowserAccessibility* text3_obj = manager->GetFromID(113);

  // Leaf nodes should have no children.
  std::vector<gfx::NativeViewAccessible> descendants =
      text1_obj->GetDescendants();
  std::vector<gfx::NativeViewAccessible> expected_descendants = {};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text2_obj->GetDescendants();
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  descendants = text3_obj->GetDescendants();
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  // Verify that para1 has two children (text1 and tex2).
  descendants = para_obj->GetDescendants();
  expected_descendants = {text1_obj->GetNativeViewAccessible(),
                          text2_obj->GetNativeViewAccessible()};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  // Calling GetChildNodeIds on the root should encompass the entire
  // right and left subtrees (para1, text1, text2, and text3).
  descendants = root_obj->GetDescendants();
  expected_descendants = {para_obj->GetNativeViewAccessible(),
                          text1_obj->GetNativeViewAccessible(),
                          text2_obj->GetNativeViewAccessible(),
                          text3_obj->GetNativeViewAccessible()};
  EXPECT_NATIVE_VIEW_ACCESSIBLE_VECTOR_EQ(descendants, expected_descendants);

  manager.reset();
}
#endif  // defined(OS_WIN) || BUILDFLAG(USE_ATK)

}  // namespace content
