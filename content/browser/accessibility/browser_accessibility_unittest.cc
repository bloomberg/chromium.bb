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

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRect) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("Hello, world.");
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 29, 18);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.SetName("Hello, ");
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.relative_bounds.bounds = gfx::RectF(100, 100, 29, 9);
  inline_text1.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(6);
  character_offsets1.push_back(11);
  character_offsets1.push_back(16);
  character_offsets1.push_back(21);
  character_offsets1.push_back(26);
  character_offsets1.push_back(29);
  character_offsets1.push_back(29);
  inline_text1.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets1);
  static_text.child_ids.push_back(3);

  ui::AXNodeData inline_text2;
  inline_text2.id = 4;
  inline_text2.SetName("world.");
  inline_text2.role = ax::mojom::Role::kInlineTextBox;
  inline_text2.relative_bounds.bounds = gfx::RectF(100, 109, 28, 9);
  inline_text2.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets2;
  character_offsets2.push_back(5);
  character_offsets2.push_back(10);
  character_offsets2.push_back(15);
  character_offsets2.push_back(20);
  character_offsets2.push_back(25);
  character_offsets2.push_back(28);
  inline_text2.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets2);
  static_text.child_ids.push_back(4);

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text1, inline_text2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

#ifdef OS_ANDROID
  // Android disallows getting inner text from root accessibility nodes.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 1, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#else
  // Validate the bounding box of 'H' from root.
  EXPECT_EQ(gfx::Rect(100, 100, 6, 9).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 1, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#endif

  // Validate the bounding box of 'H' from static text.
  EXPECT_EQ(gfx::Rect(100, 100, 6, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 1, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounding box of 'Hello' from static text.
  EXPECT_EQ(gfx::Rect(100, 100, 26, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 5, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounding box of 'Hello, world.' from static text.
  EXPECT_EQ(gfx::Rect(100, 100, 29, 18).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 13, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

#ifdef OS_ANDROID
  // Android disallows getting inner text from root accessibility nodes.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 13, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#else
  // Validate the bounding box of 'Hello, world.' from root.
  EXPECT_EQ(gfx::Rect(100, 100, 29, 18).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 13, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#endif
}

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRectMultiElement) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("ABC");
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.relative_bounds.bounds = gfx::RectF(0, 20, 33, 9);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.SetName("ABC");
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.relative_bounds.bounds = gfx::RectF(0, 20, 33, 9);
  inline_text1.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets{10, 21, 33};
  inline_text1.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets);
  static_text.child_ids.push_back(3);

  ui::AXNodeData static_text2;
  static_text2.id = 4;
  static_text2.SetName("ABC");
  static_text2.role = ax::mojom::Role::kStaticText;
  static_text2.relative_bounds.bounds = gfx::RectF(10, 40, 33, 9);
  root.child_ids.push_back(4);

  ui::AXNodeData inline_text2;
  inline_text2.id = 5;
  inline_text2.SetName("ABC");
  inline_text2.role = ax::mojom::Role::kInlineTextBox;
  inline_text2.relative_bounds.bounds = gfx::RectF(10, 40, 33, 9);
  inline_text2.SetTextDirection(ax::mojom::TextDirection::kLtr);
  inline_text2.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets);
  static_text2.child_ids.push_back(5);

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text1, static_text2,
                           inline_text2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);
  BrowserAccessibility* static_text_accessible2 =
      root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, static_text_accessible);

  // Validate the bounds of 'ABC' on the first line.
  EXPECT_EQ(gfx::Rect(0, 20, 33, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 3, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounds of only 'AB' on the first line.
  EXPECT_EQ(gfx::Rect(0, 20, 21, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 2, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounds of only 'BC' on the first line.
  EXPECT_EQ(gfx::Rect(10, 20, 23, 9).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    1, 3, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounds of 'ABC' on the second line.
  EXPECT_EQ(gfx::Rect(10, 40, 33, 9).ToString(),
            static_text_accessible2
                ->GetInnerTextRangeBoundsRect(
                    0, 3, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

#ifdef OS_ANDROID
  // Android disallows getting inner text from accessibility root nodes.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 6, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Android disallows getting inner text from accessibility root nodes.
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    2, 4, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#else
  // Validate the bounds of 'ABCABC' from both lines.
  EXPECT_EQ(gfx::Rect(0, 20, 43, 29).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 6, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // Validate the bounds of 'CA' from both lines.
  EXPECT_EQ(gfx::Rect(10, 20, 23, 29).ToString(),
            root_accessible
                ->GetInnerTextRangeBoundsRect(
                    2, 4, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
#endif
}

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRectBiDi) {
  // In this example, we assume that the string "123abc" is rendered with "123"
  // going left-to-right and "abc" going right-to-left. In other words,
  // on-screen it would look like "123cba".
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("123abc");
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 60, 20);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.SetName("123");
  inline_text1.role = ax::mojom::Role::kInlineTextBox;
  inline_text1.relative_bounds.bounds = gfx::RectF(100, 100, 30, 20);
  inline_text1.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(10);  // 0
  character_offsets1.push_back(20);  // 1
  character_offsets1.push_back(30);  // 2
  inline_text1.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets1);
  static_text.child_ids.push_back(3);

  ui::AXNodeData inline_text2;
  inline_text2.id = 4;
  inline_text2.SetName("abc");
  inline_text2.role = ax::mojom::Role::kInlineTextBox;
  inline_text2.relative_bounds.bounds = gfx::RectF(130, 100, 30, 20);
  inline_text2.SetTextDirection(ax::mojom::TextDirection::kRtl);
  std::vector<int32_t> character_offsets2;
  character_offsets2.push_back(10);
  character_offsets2.push_back(20);
  character_offsets2.push_back(30);
  inline_text2.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets2);
  static_text.child_ids.push_back(4);

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text1, inline_text2),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

  EXPECT_EQ(gfx::Rect(100, 100, 60, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 6, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 10, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 1, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 30, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    0, 3, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  EXPECT_EQ(gfx::Rect(150, 100, 10, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    3, 4, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  EXPECT_EQ(gfx::Rect(130, 100, 30, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    3, 6, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());

  // This range is only two characters, but because of the direction switch
  // the bounds are as wide as four characters.
  EXPECT_EQ(gfx::Rect(120, 100, 40, 20).ToString(),
            static_text_accessible
                ->GetInnerTextRangeBoundsRect(
                    2, 4, ui::AXCoordinateSystem::kRootFrame,
                    ui::AXClippingBehavior::kUnclipped)
                .ToString());
}

TEST_F(BrowserAccessibilityTest, GetInnerTextRangeBoundsRectScrolledWindow) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.AddIntAttribute(ax::mojom::IntAttribute::kScrollX, 25);
  root.AddIntAttribute(ax::mojom::IntAttribute::kScrollY, 50);
  root.relative_bounds.bounds = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("ABC");
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.relative_bounds.bounds = gfx::RectF(100, 100, 16, 9);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text;
  inline_text.id = 3;
  inline_text.SetName("ABC");
  inline_text.role = ax::mojom::Role::kInlineTextBox;
  inline_text.relative_bounds.bounds = gfx::RectF(100, 100, 16, 9);
  inline_text.SetTextDirection(ax::mojom::TextDirection::kLtr);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(6);   // 0
  character_offsets1.push_back(11);  // 1
  character_offsets1.push_back(16);  // 2
  inline_text.AddIntListAttribute(
      ax::mojom::IntListAttribute::kCharacterOffsets, character_offsets1);
  static_text.child_ids.push_back(3);

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text),
          test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

  if (browser_accessibility_manager
          ->UseRootScrollOffsetsWhenComputingBounds()) {
    EXPECT_EQ(gfx::Rect(75, 50, 16, 9).ToString(),
              static_text_accessible
                  ->GetInnerTextRangeBoundsRect(
                      0, 3, ui::AXCoordinateSystem::kRootFrame,
                      ui::AXClippingBehavior::kUnclipped)
                  .ToString());
  } else {
    EXPECT_EQ(gfx::Rect(100, 100, 16, 9).ToString(),
              static_text_accessible
                  ->GetInnerTextRangeBoundsRect(
                      0, 3, ui::AXCoordinateSystem::kRootFrame,
                      ui::AXClippingBehavior::kUnclipped)
                  .ToString());
  }
}

TEST_F(BrowserAccessibilityTest, GetAuthorUniqueId) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ax::mojom::Role::kRootWebArea;
  root.html_attributes.push_back(std::make_pair("id", "my_html_id"));

  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root), test_browser_accessibility_delegate_.get(),
          new BrowserAccessibilityFactory()));
  ASSERT_NE(nullptr, browser_accessibility_manager.get());

  BrowserAccessibility* root_accessible =
      browser_accessibility_manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);

  ASSERT_EQ(base::WideToUTF16(L"my_html_id"),
            root_accessible->GetAuthorUniqueId());
}
}  // namespace content
