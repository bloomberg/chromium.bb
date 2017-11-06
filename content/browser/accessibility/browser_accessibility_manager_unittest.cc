// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#if defined(OS_WIN)
#include "content/browser/accessibility/browser_accessibility_win.h"
#endif
#include "content/public/browser/ax_event_notification_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// Subclass of BrowserAccessibility that counts the number of instances.
class CountedBrowserAccessibility : public BrowserAccessibility {
 public:
  CountedBrowserAccessibility() {
    global_obj_count_++;
    native_ref_count_ = 1;
  }
  ~CountedBrowserAccessibility() override { global_obj_count_--; }

  void NativeAddReference() override { native_ref_count_++; }

  void NativeReleaseReference() override {
    native_ref_count_--;
    if (native_ref_count_ == 0)
      delete this;
  }

  int native_ref_count_;
  static int global_obj_count_;
};

int CountedBrowserAccessibility::global_obj_count_ = 0;

// Factory that creates a CountedBrowserAccessibility.
class CountedBrowserAccessibilityFactory
    : public BrowserAccessibilityFactory {
 public:
  ~CountedBrowserAccessibilityFactory() override {}
  BrowserAccessibility* Create() override {
    return new CountedBrowserAccessibility();
  }
};

class TestBrowserAccessibilityDelegate
    : public BrowserAccessibilityDelegate {
 public:
  TestBrowserAccessibilityDelegate()
      : got_fatal_error_(false) {}

  void AccessibilityPerformAction(const ui::AXActionData& data) override {}
  bool AccessibilityViewHasFocus() const override { return false; }
  gfx::Rect AccessibilityGetViewBounds() const override { return gfx::Rect(); }
  gfx::Point AccessibilityOriginInScreen(
      const gfx::Rect& bounds) const override {
    return gfx::Point();
  }
  float AccessibilityGetDeviceScaleFactor() const override { return 1.0f; }
  void AccessibilityFatalError() override { got_fatal_error_ = true; }
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override {
    return gfx::kNullAcceleratedWidget;
  }
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override {
    return nullptr;
  }

  bool got_fatal_error() const { return got_fatal_error_; }
  void reset_got_fatal_error() { got_fatal_error_ = false; }

private:
  bool got_fatal_error_;
};

}  // anonymous namespace

TEST(BrowserAccessibilityManagerTest, TestNoLeaks) {
  // Create ui::AXNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  ui::AXNodeData button;
  button.id = 2;
  button.SetName("Button");
  button.role = ui::AX_ROLE_BUTTON;

  ui::AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.SetName("Checkbox");
  checkbox.role = ui::AX_ROLE_CHECK_BOX;

  ui::AXNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  // Construct a BrowserAccessibilityManager with this
  // ui::AXNodeData tree and a factory for an instance-counting
  // BrowserAccessibility, and ensure that exactly 3 instances were
  // created. Note that the manager takes ownership of the factory.
  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager =
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, button, checkbox),
          nullptr,
          new CountedBrowserAccessibilityFactory());

  ASSERT_EQ(3, CountedBrowserAccessibility::global_obj_count_);

  // Delete the manager and test that all 3 instances are deleted.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);

  // Construct a manager again, and this time save references to two of
  // the three nodes in the tree.
  manager =
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, button, checkbox),
          nullptr,
          new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(3, CountedBrowserAccessibility::global_obj_count_);

  CountedBrowserAccessibility* root_accessible =
      static_cast<CountedBrowserAccessibility*>(manager->GetRoot());
  root_accessible->NativeAddReference();
  CountedBrowserAccessibility* child1_accessible =
      static_cast<CountedBrowserAccessibility*>(
          root_accessible->PlatformGetChild(1));
  child1_accessible->NativeAddReference();

  // Now delete the manager, and only one of the three nodes in the tree
  // should be released.
  delete manager;
  ASSERT_EQ(2, CountedBrowserAccessibility::global_obj_count_);

  // Release each of our references and make sure that each one results in
  // the instance being deleted as its reference count hits zero.
  root_accessible->NativeReleaseReference();
  ASSERT_EQ(1, CountedBrowserAccessibility::global_obj_count_);
  child1_accessible->NativeReleaseReference();
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}

TEST(BrowserAccessibilityManagerTest, TestReuseBrowserAccessibilityObjects) {
  // Make sure that changes to a subtree reuse as many objects as possible.

  // Tree 1:
  //
  // root
  //   child1
  //   child2
  //   child3

  ui::AXNodeData tree1_child1;
  tree1_child1.id = 2;
  tree1_child1.SetName("Child1");
  tree1_child1.role = ui::AX_ROLE_BUTTON;

  ui::AXNodeData tree1_child2;
  tree1_child2.id = 3;
  tree1_child2.SetName("Child2");
  tree1_child2.role = ui::AX_ROLE_BUTTON;

  ui::AXNodeData tree1_child3;
  tree1_child3.id = 4;
  tree1_child3.SetName("Child3");
  tree1_child3.role = ui::AX_ROLE_BUTTON;

  ui::AXNodeData tree1_root;
  tree1_root.id = 1;
  tree1_root.SetName("Document");
  tree1_root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  tree1_root.child_ids.push_back(2);
  tree1_root.child_ids.push_back(3);
  tree1_root.child_ids.push_back(4);

  // Tree 2:
  //
  // root
  //   child0  <-- inserted
  //   child1
  //   child2
  //           <-- child3 deleted

  ui::AXNodeData tree2_child0;
  tree2_child0.id = 5;
  tree2_child0.SetName("Child0");
  tree2_child0.role = ui::AX_ROLE_BUTTON;

  ui::AXNodeData tree2_root;
  tree2_root.id = 1;
  tree2_root.SetName("DocumentChanged");
  tree2_root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  tree2_root.child_ids.push_back(5);
  tree2_root.child_ids.push_back(2);
  tree2_root.child_ids.push_back(3);

  // Construct a BrowserAccessibilityManager with tree1.
  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager =
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(tree1_root,
                           tree1_child1, tree1_child2, tree1_child3),
          nullptr,
          new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(4, CountedBrowserAccessibility::global_obj_count_);

  // Save references to all of the objects.
  CountedBrowserAccessibility* root_accessible =
      static_cast<CountedBrowserAccessibility*>(manager->GetRoot());
  root_accessible->NativeAddReference();
  CountedBrowserAccessibility* child1_accessible =
      static_cast<CountedBrowserAccessibility*>(
          root_accessible->PlatformGetChild(0));
  child1_accessible->NativeAddReference();
  CountedBrowserAccessibility* child2_accessible =
      static_cast<CountedBrowserAccessibility*>(
          root_accessible->PlatformGetChild(1));
  child2_accessible->NativeAddReference();
  CountedBrowserAccessibility* child3_accessible =
      static_cast<CountedBrowserAccessibility*>(
          root_accessible->PlatformGetChild(2));
  child3_accessible->NativeAddReference();

  // Check the index in parent.
  EXPECT_EQ(0, child1_accessible->GetIndexInParent());
  EXPECT_EQ(1, child2_accessible->GetIndexInParent());
  EXPECT_EQ(2, child3_accessible->GetIndexInParent());

  // Process a notification containing the changed subtree.
  std::vector<AXEventNotificationDetails> params;
  params.push_back(AXEventNotificationDetails());
  AXEventNotificationDetails* msg = &params[0];
  msg->event_type = ui::AX_EVENT_CHILDREN_CHANGED;
  msg->update.nodes.push_back(tree2_root);
  msg->update.nodes.push_back(tree2_child0);
  msg->id = tree2_root.id;
  manager->OnAccessibilityEvents(params);

  // There should be 5 objects now: the 4 from the new tree, plus the
  // reference to child3 we kept.
  EXPECT_EQ(5, CountedBrowserAccessibility::global_obj_count_);

  // Check that our references to the root, child1, and child2 are still valid,
  // but that the reference to child3 is now invalid.
  EXPECT_TRUE(root_accessible->instance_active());
  EXPECT_TRUE(child1_accessible->instance_active());
  EXPECT_TRUE(child2_accessible->instance_active());
  EXPECT_FALSE(child3_accessible->instance_active());

  // Check that the index in parent has been updated.
  EXPECT_EQ(1, child1_accessible->GetIndexInParent());
  EXPECT_EQ(2, child2_accessible->GetIndexInParent());

  // Release our references. The object count should only decrease by 1
  // for child3.
  root_accessible->NativeReleaseReference();
  child1_accessible->NativeReleaseReference();
  child2_accessible->NativeReleaseReference();
  child3_accessible->NativeReleaseReference();

  EXPECT_EQ(4, CountedBrowserAccessibility::global_obj_count_);

  // Delete the manager and make sure all memory is cleaned up.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}

TEST(BrowserAccessibilityManagerTest, TestReuseBrowserAccessibilityObjects2) {
  // Similar to the test above, but with a more complicated tree.

  // Tree 1:
  //
  // root
  //   container
  //     child1
  //       grandchild1
  //     child2
  //       grandchild2
  //     child3
  //       grandchild3

  ui::AXNodeData tree1_grandchild1;
  tree1_grandchild1.id = 4;
  tree1_grandchild1.SetName("GrandChild1");
  tree1_grandchild1.role = ui::AX_ROLE_BUTTON;

  ui::AXNodeData tree1_child1;
  tree1_child1.id = 3;
  tree1_child1.SetName("Child1");
  tree1_child1.role = ui::AX_ROLE_BUTTON;
  tree1_child1.child_ids.push_back(4);

  ui::AXNodeData tree1_grandchild2;
  tree1_grandchild2.id = 6;
  tree1_grandchild2.SetName("GrandChild1");
  tree1_grandchild2.role = ui::AX_ROLE_BUTTON;

  ui::AXNodeData tree1_child2;
  tree1_child2.id = 5;
  tree1_child2.SetName("Child2");
  tree1_child2.role = ui::AX_ROLE_BUTTON;
  tree1_child2.child_ids.push_back(6);

  ui::AXNodeData tree1_grandchild3;
  tree1_grandchild3.id = 8;
  tree1_grandchild3.SetName("GrandChild3");
  tree1_grandchild3.role = ui::AX_ROLE_BUTTON;

  ui::AXNodeData tree1_child3;
  tree1_child3.id = 7;
  tree1_child3.SetName("Child3");
  tree1_child3.role = ui::AX_ROLE_BUTTON;
  tree1_child3.child_ids.push_back(8);

  ui::AXNodeData tree1_container;
  tree1_container.id = 2;
  tree1_container.SetName("Container");
  tree1_container.role = ui::AX_ROLE_GROUP;
  tree1_container.child_ids.push_back(3);
  tree1_container.child_ids.push_back(5);
  tree1_container.child_ids.push_back(7);

  ui::AXNodeData tree1_root;
  tree1_root.id = 1;
  tree1_root.SetName("Document");
  tree1_root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  tree1_root.child_ids.push_back(2);

  // Tree 2:
  //
  // root
  //   container
  //     child0         <-- inserted
  //       grandchild0  <--
  //     child1
  //       grandchild1
  //     child2
  //       grandchild2
  //                    <-- child3 (and grandchild3) deleted

  ui::AXNodeData tree2_grandchild0;
  tree2_grandchild0.id = 9;
  tree2_grandchild0.SetName("GrandChild0");
  tree2_grandchild0.role = ui::AX_ROLE_BUTTON;

  ui::AXNodeData tree2_child0;
  tree2_child0.id = 10;
  tree2_child0.SetName("Child0");
  tree2_child0.role = ui::AX_ROLE_BUTTON;
  tree2_child0.child_ids.push_back(9);

  ui::AXNodeData tree2_container;
  tree2_container.id = 2;
  tree2_container.SetName("Container");
  tree2_container.role = ui::AX_ROLE_GROUP;
  tree2_container.child_ids.push_back(10);
  tree2_container.child_ids.push_back(3);
  tree2_container.child_ids.push_back(5);

  // Construct a BrowserAccessibilityManager with tree1.
  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager =
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(tree1_root, tree1_container,
                           tree1_child1, tree1_grandchild1,
                           tree1_child2, tree1_grandchild2,
                           tree1_child3, tree1_grandchild3),
          nullptr,
          new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(8, CountedBrowserAccessibility::global_obj_count_);

  // Save references to some objects.
  CountedBrowserAccessibility* root_accessible =
      static_cast<CountedBrowserAccessibility*>(manager->GetRoot());
  root_accessible->NativeAddReference();
  CountedBrowserAccessibility* container_accessible =
      static_cast<CountedBrowserAccessibility*>(
          root_accessible->PlatformGetChild(0));
  container_accessible->NativeAddReference();
  CountedBrowserAccessibility* child2_accessible =
      static_cast<CountedBrowserAccessibility*>(
          container_accessible->PlatformGetChild(1));
  child2_accessible->NativeAddReference();
  CountedBrowserAccessibility* child3_accessible =
      static_cast<CountedBrowserAccessibility*>(
          container_accessible->PlatformGetChild(2));
  child3_accessible->NativeAddReference();

  // Check the index in parent.
  EXPECT_EQ(1, child2_accessible->GetIndexInParent());
  EXPECT_EQ(2, child3_accessible->GetIndexInParent());

  // Process a notification containing the changed subtree rooted at
  // the container.
  std::vector<AXEventNotificationDetails> params;
  params.push_back(AXEventNotificationDetails());
  AXEventNotificationDetails* msg = &params[0];
  msg->event_type = ui::AX_EVENT_CHILDREN_CHANGED;
  msg->update.nodes.push_back(tree2_container);
  msg->update.nodes.push_back(tree2_child0);
  msg->update.nodes.push_back(tree2_grandchild0);
  msg->id = tree2_container.id;
  manager->OnAccessibilityEvents(params);

  // There should be 9 objects now: the 8 from the new tree, plus the
  // reference to child3 we kept.
  EXPECT_EQ(9, CountedBrowserAccessibility::global_obj_count_);

  // Check that our references to the root and container and child2 are
  // still valid, but that the reference to child3 is now invalid.
  EXPECT_TRUE(root_accessible->instance_active());
  EXPECT_TRUE(container_accessible->instance_active());
  EXPECT_TRUE(child2_accessible->instance_active());
  EXPECT_FALSE(child3_accessible->instance_active());

  // Ensure that we retain the parent of the detached subtree.
  EXPECT_EQ(root_accessible, container_accessible->PlatformGetParent());
  EXPECT_EQ(0, container_accessible->GetIndexInParent());

  // Check that the index in parent has been updated.
  EXPECT_EQ(2, child2_accessible->GetIndexInParent());

  // Release our references. The object count should only decrease by 1
  // for child3.
  root_accessible->NativeReleaseReference();
  container_accessible->NativeReleaseReference();
  child2_accessible->NativeReleaseReference();
  child3_accessible->NativeReleaseReference();

  EXPECT_EQ(8, CountedBrowserAccessibility::global_obj_count_);

  // Delete the manager and make sure all memory is cleaned up.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}

TEST(BrowserAccessibilityManagerTest, TestMoveChildUp) {
  // Tree 1:
  //
  // 1
  //   2
  //   3
  //     4

  ui::AXNodeData tree1_4;
  tree1_4.id = 4;

  ui::AXNodeData tree1_3;
  tree1_3.id = 3;
  tree1_3.child_ids.push_back(4);

  ui::AXNodeData tree1_2;
  tree1_2.id = 2;

  ui::AXNodeData tree1_1;
  tree1_1.id = 1;
  tree1_1.role = ui::AX_ROLE_ROOT_WEB_AREA;
  tree1_1.child_ids.push_back(2);
  tree1_1.child_ids.push_back(3);

  // Tree 2:
  //
  // 1
  //   4    <-- moves up a level and gains child
  //     6  <-- new
  //   5    <-- new

  ui::AXNodeData tree2_6;
  tree2_6.id = 6;

  ui::AXNodeData tree2_5;
  tree2_5.id = 5;

  ui::AXNodeData tree2_4;
  tree2_4.id = 4;
  tree2_4.child_ids.push_back(6);

  ui::AXNodeData tree2_1;
  tree2_1.id = 1;
  tree2_1.child_ids.push_back(4);
  tree2_1.child_ids.push_back(5);

  // Construct a BrowserAccessibilityManager with tree1.
  CountedBrowserAccessibility::global_obj_count_ = 0;
  BrowserAccessibilityManager* manager =
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(tree1_1, tree1_2, tree1_3, tree1_4),
          nullptr,
          new CountedBrowserAccessibilityFactory());
  ASSERT_EQ(4, CountedBrowserAccessibility::global_obj_count_);

  // Process a notification containing the changed subtree.
  std::vector<AXEventNotificationDetails> params;
  params.push_back(AXEventNotificationDetails());
  AXEventNotificationDetails* msg = &params[0];
  msg->event_type = ui::AX_EVENT_CHILDREN_CHANGED;
  msg->update.nodes.push_back(tree2_1);
  msg->update.nodes.push_back(tree2_4);
  msg->update.nodes.push_back(tree2_5);
  msg->update.nodes.push_back(tree2_6);
  msg->id = tree2_1.id;
  manager->OnAccessibilityEvents(params);

  // There should be 4 objects now.
  EXPECT_EQ(4, CountedBrowserAccessibility::global_obj_count_);

  // Delete the manager and make sure all memory is cleaned up.
  delete manager;
  ASSERT_EQ(0, CountedBrowserAccessibility::global_obj_count_);
}

// Temporarily disabled due to bug http://crbug.com/765490
TEST(BrowserAccessibilityManagerTest, DISABLED_TestFatalError) {
  // Test that BrowserAccessibilityManager raises a fatal error
  // (which will crash the renderer) if the same id is used in
  // two places in the tree.

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.child_ids.push_back(2);
  root.child_ids.push_back(2);

  CountedBrowserAccessibilityFactory* factory =
      new CountedBrowserAccessibilityFactory();
  std::unique_ptr<TestBrowserAccessibilityDelegate> delegate(
      new TestBrowserAccessibilityDelegate());
  std::unique_ptr<BrowserAccessibilityManager> manager;
  ASSERT_FALSE(delegate->got_fatal_error());
  manager.reset(BrowserAccessibilityManager::Create(
      MakeAXTreeUpdate(root),
      delegate.get(),
      factory));
  ASSERT_TRUE(delegate->got_fatal_error());

  ui::AXNodeData root2;
  root2.id = 1;
  root2.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root2.child_ids.push_back(2);
  root2.child_ids.push_back(3);

  ui::AXNodeData child1;
  child1.id = 2;
  child1.child_ids.push_back(4);
  child1.child_ids.push_back(5);

  ui::AXNodeData child2;
  child2.id = 3;
  child2.child_ids.push_back(6);
  child2.child_ids.push_back(5);  // Duplicate

  ui::AXNodeData grandchild4;
  grandchild4.id = 4;

  ui::AXNodeData grandchild5;
  grandchild5.id = 5;

  ui::AXNodeData grandchild6;
  grandchild6.id = 6;

  delegate->reset_got_fatal_error();
  factory = new CountedBrowserAccessibilityFactory();
  manager.reset(BrowserAccessibilityManager::Create(
      MakeAXTreeUpdate(root2, child1, child2,
                       grandchild4, grandchild5, grandchild6),
      delegate.get(),
      factory));
  ASSERT_TRUE(delegate->got_fatal_error());
}

TEST(BrowserAccessibilityManagerTest, BoundsForRange) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.location = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("Hello, world.");
  static_text.role = ui::AX_ROLE_STATIC_TEXT;
  static_text.location = gfx::RectF(100, 100, 29, 18);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.SetName("Hello, ");
  inline_text1.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_text1.location = gfx::RectF(100, 100, 29, 9);
  inline_text1.AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                               ui::AX_TEXT_DIRECTION_LTR);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(6);   // 0
  character_offsets1.push_back(11);  // 1
  character_offsets1.push_back(16);  // 2
  character_offsets1.push_back(21);  // 3
  character_offsets1.push_back(26);  // 4
  character_offsets1.push_back(29);  // 5
  character_offsets1.push_back(29);  // 6 (note that the space has no width)
  inline_text1.AddIntListAttribute(
      ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets1);
  static_text.child_ids.push_back(3);

  ui::AXNodeData inline_text2;
  inline_text2.id = 4;
  inline_text2.SetName("world.");
  inline_text2.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_text2.location = gfx::RectF(100, 109, 28, 9);
  inline_text2.AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                               ui::AX_TEXT_DIRECTION_LTR);
  std::vector<int32_t> character_offsets2;
  character_offsets2.push_back(5);
  character_offsets2.push_back(10);
  character_offsets2.push_back(15);
  character_offsets2.push_back(20);
  character_offsets2.push_back(25);
  character_offsets2.push_back(28);
  inline_text2.AddIntListAttribute(
      ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets2);
  static_text.child_ids.push_back(4);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text1, inline_text2),
          nullptr, new CountedBrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

  EXPECT_EQ(gfx::Rect(100, 100, 6, 9).ToString(),
            static_text_accessible->GetPageBoundsForRange(0, 1).ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 26, 9).ToString(),
            static_text_accessible->GetPageBoundsForRange(0, 5).ToString());

  EXPECT_EQ(gfx::Rect(100, 109, 5, 9).ToString(),
            static_text_accessible->GetPageBoundsForRange(7, 1).ToString());

  EXPECT_EQ(gfx::Rect(100, 109, 25, 9).ToString(),
            static_text_accessible->GetPageBoundsForRange(7, 5).ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 29, 18).ToString(),
            static_text_accessible->GetPageBoundsForRange(5, 3).ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 29, 18).ToString(),
            static_text_accessible->GetPageBoundsForRange(0, 13).ToString());

  // Note that each child in the parent element is represented by a single
  // embedded object character and not by its text.
  // TODO(nektar): Investigate failure on Linux.
  EXPECT_EQ(gfx::Rect(100, 100, 29, 18).ToString(),
            root_accessible->GetPageBoundsForRange(0, 13).ToString());
}

TEST(BrowserAccessibilityManagerTest, BoundsForRangeMultiElement) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.location = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("ABC");
  static_text.role = ui::AX_ROLE_STATIC_TEXT;
  static_text.location = gfx::RectF(0, 20, 33, 9);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.SetName("ABC");
  inline_text1.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_text1.location = gfx::RectF(0, 20, 33, 9);
  inline_text1.AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                               ui::AX_TEXT_DIRECTION_LTR);
  std::vector<int32_t> character_offsets { 10, 21, 33 };
  inline_text1.AddIntListAttribute(
      ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets);
  static_text.child_ids.push_back(3);

  ui::AXNodeData static_text2;
  static_text2.id = 4;
  static_text2.SetName("ABC");
  static_text2.role = ui::AX_ROLE_STATIC_TEXT;
  static_text2.location = gfx::RectF(10, 40, 33, 9);
  root.child_ids.push_back(4);

  ui::AXNodeData inline_text2;
  inline_text2.id = 5;
  inline_text2.SetName("ABC");
  inline_text2.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_text2.location = gfx::RectF(10, 40, 33, 9);
  inline_text2.AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                               ui::AX_TEXT_DIRECTION_LTR);
  inline_text2.AddIntListAttribute(
      ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets);
  static_text2.child_ids.push_back(5);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(
              root, static_text, inline_text1, static_text2, inline_text2),
          nullptr, new CountedBrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);
  BrowserAccessibility* static_text_accessible2 =
      root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, static_text_accessible);

  // The first line.
  EXPECT_EQ(gfx::Rect(0, 20, 33, 9).ToString(),
            manager->GetPageBoundsForRange(
                *static_text_accessible, 0,
                *static_text_accessible, 3).ToString());

  // Part of the first line.
  EXPECT_EQ(gfx::Rect(0, 20, 21, 9).ToString(),
            manager->GetPageBoundsForRange(
                *static_text_accessible, 0,
                *static_text_accessible, 2).ToString());

  // Part of the first line.
  EXPECT_EQ(gfx::Rect(10, 20, 23, 9).ToString(),
            manager->GetPageBoundsForRange(
                *static_text_accessible, 1,
                *static_text_accessible, 3).ToString());

  // The second line.
  EXPECT_EQ(gfx::Rect(10, 40, 33, 9).ToString(),
            manager->GetPageBoundsForRange(
                *static_text_accessible2, 0,
                *static_text_accessible2, 3).ToString());

  // All of both lines.
  EXPECT_EQ(gfx::Rect(0, 20, 43, 29).ToString(),
            manager->GetPageBoundsForRange(
                *static_text_accessible, 0,
                *static_text_accessible2, 3).ToString());

  // Part of both lines.
  EXPECT_EQ(gfx::Rect(10, 20, 23, 29).ToString(),
            manager->GetPageBoundsForRange(
                *static_text_accessible, 2,
                *static_text_accessible2, 1).ToString());

  // Part of both lines in reverse order.
  EXPECT_EQ(gfx::Rect(10, 20, 23, 29).ToString(),
            manager->GetPageBoundsForRange(
                *static_text_accessible2, 1,
                *static_text_accessible, 2).ToString());
}

TEST(BrowserAccessibilityManagerTest, BoundsForRangeBiDi) {
  // In this example, we assume that the string "123abc" is rendered with
  // "123" going left-to-right and "abc" going right-to-left. In other
  // words, on-screen it would look like "123cba". This is possible to
  // achieve if the source string had unicode control characters
  // to switch directions. This test doesn't worry about how, though - it just
  // tests that if something like that were to occur, GetPageBoundsForRange
  // returns the correct bounds for different ranges.

  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.location = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("123abc");
  static_text.role = ui::AX_ROLE_STATIC_TEXT;
  static_text.location = gfx::RectF(100, 100, 60, 20);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text1;
  inline_text1.id = 3;
  inline_text1.SetName("123");
  inline_text1.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_text1.location = gfx::RectF(100, 100, 30, 20);
  inline_text1.AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                               ui::AX_TEXT_DIRECTION_LTR);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(10);  // 0
  character_offsets1.push_back(20);  // 1
  character_offsets1.push_back(30);  // 2
  inline_text1.AddIntListAttribute(
      ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets1);
  static_text.child_ids.push_back(3);

  ui::AXNodeData inline_text2;
  inline_text2.id = 4;
  inline_text2.SetName("abc");
  inline_text2.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_text2.location = gfx::RectF(130, 100, 30, 20);
  inline_text2.AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                               ui::AX_TEXT_DIRECTION_RTL);
  std::vector<int32_t> character_offsets2;
  character_offsets2.push_back(10);
  character_offsets2.push_back(20);
  character_offsets2.push_back(30);
  inline_text2.AddIntListAttribute(
      ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets2);
  static_text.child_ids.push_back(4);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text1, inline_text2),
          nullptr, new CountedBrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

  EXPECT_EQ(gfx::Rect(100, 100, 60, 20).ToString(),
            static_text_accessible->GetPageBoundsForRange(0, 6).ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 10, 20).ToString(),
            static_text_accessible->GetPageBoundsForRange(0, 1).ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 30, 20).ToString(),
            static_text_accessible->GetPageBoundsForRange(0, 3).ToString());

  EXPECT_EQ(gfx::Rect(150, 100, 10, 20).ToString(),
            static_text_accessible->GetPageBoundsForRange(3, 1).ToString());

  EXPECT_EQ(gfx::Rect(130, 100, 30, 20).ToString(),
            static_text_accessible->GetPageBoundsForRange(3, 3).ToString());

  // This range is only two characters, but because of the direction switch
  // the bounds are as wide as four characters.
  EXPECT_EQ(gfx::Rect(120, 100, 40, 20).ToString(),
            static_text_accessible->GetPageBoundsForRange(2, 2).ToString());
}

TEST(BrowserAccessibilityManagerTest, BoundsForRangeScrolledWindow) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.AddIntAttribute(ui::AX_ATTR_SCROLL_X, 25);
  root.AddIntAttribute(ui::AX_ATTR_SCROLL_Y, 50);
  root.location = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData static_text;
  static_text.id = 2;
  static_text.SetName("ABC");
  static_text.role = ui::AX_ROLE_STATIC_TEXT;
  static_text.location = gfx::RectF(100, 100, 16, 9);
  root.child_ids.push_back(2);

  ui::AXNodeData inline_text;
  inline_text.id = 3;
  inline_text.SetName("ABC");
  inline_text.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_text.location = gfx::RectF(100, 100, 16, 9);
  inline_text.AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                              ui::AX_TEXT_DIRECTION_LTR);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(6);   // 0
  character_offsets1.push_back(11);  // 1
  character_offsets1.push_back(16);  // 2
  inline_text.AddIntListAttribute(
      ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets1);
  static_text.child_ids.push_back(3);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, static_text, inline_text), nullptr,
          new CountedBrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* static_text_accessible =
      root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, static_text_accessible);

  if (manager->UseRootScrollOffsetsWhenComputingBounds()) {
    EXPECT_EQ(gfx::Rect(75, 50, 16, 9).ToString(),
              static_text_accessible->GetPageBoundsForRange(0, 3).ToString());
  } else {
    EXPECT_EQ(gfx::Rect(100, 100, 16, 9).ToString(),
              static_text_accessible->GetPageBoundsForRange(0, 3).ToString());
  }
}

TEST(BrowserAccessibilityManagerTest, BoundsForRangeOnParentElement) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.child_ids.push_back(2);
  root.location = gfx::RectF(0, 0, 800, 600);

  ui::AXNodeData div;
  div.id = 2;
  div.role = ui::AX_ROLE_GENERIC_CONTAINER;
  div.location = gfx::RectF(100, 100, 100, 20);
  div.child_ids.push_back(3);
  div.child_ids.push_back(4);
  div.child_ids.push_back(5);

  ui::AXNodeData static_text1;
  static_text1.id = 3;
  static_text1.SetName("AB");
  static_text1.role = ui::AX_ROLE_STATIC_TEXT;
  static_text1.location = gfx::RectF(100, 100, 40, 20);
  static_text1.child_ids.push_back(6);

  ui::AXNodeData img;
  img.id = 4;
  img.SetName("Test image");
  img.role = ui::AX_ROLE_IMAGE;
  img.location = gfx::RectF(140, 100, 20, 20);

  ui::AXNodeData static_text2;
  static_text2.id = 5;
  static_text2.SetName("CD");
  static_text2.role = ui::AX_ROLE_STATIC_TEXT;
  static_text2.location = gfx::RectF(160, 100, 40, 20);
  static_text2.child_ids.push_back(7);

  ui::AXNodeData inline_text1;
  inline_text1.id = 6;
  inline_text1.SetName("AB");
  inline_text1.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_text1.location = gfx::RectF(100, 100, 40, 20);
  inline_text1.AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                               ui::AX_TEXT_DIRECTION_LTR);
  std::vector<int32_t> character_offsets1;
  character_offsets1.push_back(20);  // 0
  character_offsets1.push_back(40);  // 1
  inline_text1.AddIntListAttribute(
      ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets1);

  ui::AXNodeData inline_text2;
  inline_text2.id = 7;
  inline_text2.SetName("CD");
  inline_text2.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  inline_text2.location = gfx::RectF(160, 100, 40, 20);
  inline_text2.AddIntAttribute(ui::AX_ATTR_TEXT_DIRECTION,
                               ui::AX_TEXT_DIRECTION_LTR);
  std::vector<int32_t> character_offsets2;
  character_offsets2.push_back(20);  // 0
  character_offsets2.push_back(40);  // 1
  inline_text2.AddIntListAttribute(
      ui::AX_ATTR_CHARACTER_OFFSETS, character_offsets2);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, div, static_text1, img, static_text2,
                           inline_text1, inline_text2),
          nullptr, new CountedBrowserAccessibilityFactory()));
  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  BrowserAccessibility* div_accessible = root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, div_accessible);

  EXPECT_EQ(gfx::Rect(100, 100, 20, 20).ToString(),
            div_accessible->GetPageBoundsForRange(0, 1).ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 40, 20).ToString(),
            div_accessible->GetPageBoundsForRange(0, 2).ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 80, 20).ToString(),
            div_accessible->GetPageBoundsForRange(0, 4).ToString());

  EXPECT_EQ(gfx::Rect(120, 100, 60, 20).ToString(),
            div_accessible->GetPageBoundsForRange(1, 3).ToString());

  EXPECT_EQ(gfx::Rect(120, 100, 80, 20).ToString(),
            div_accessible->GetPageBoundsForRange(1, 4).ToString());

  EXPECT_EQ(gfx::Rect(100, 100, 100, 20).ToString(),
            div_accessible->GetPageBoundsForRange(0, 5).ToString());
}

TEST(BrowserAccessibilityManagerTest, TestNextPreviousInTreeOrder) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;

  ui::AXNodeData node2;
  node2.id = 2;
  root.child_ids.push_back(2);

  ui::AXNodeData node3;
  node3.id = 3;
  root.child_ids.push_back(3);

  ui::AXNodeData node4;
  node4.id = 4;
  node3.child_ids.push_back(4);

  ui::AXNodeData node5;
  node5.id = 5;
  root.child_ids.push_back(5);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, node2, node3, node4, node5), nullptr,
          new CountedBrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(3U, root_accessible->PlatformChildCount());
  BrowserAccessibility* node2_accessible = root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, node2_accessible);
  BrowserAccessibility* node3_accessible = root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, node3_accessible);
  ASSERT_EQ(1U, node3_accessible->PlatformChildCount());
  BrowserAccessibility* node4_accessible =
      node3_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, node4_accessible);
  BrowserAccessibility* node5_accessible = root_accessible->PlatformGetChild(2);
  ASSERT_NE(nullptr, node5_accessible);

  EXPECT_EQ(nullptr, manager->NextInTreeOrder(nullptr));
  EXPECT_EQ(node2_accessible, manager->NextInTreeOrder(root_accessible));
  EXPECT_EQ(node3_accessible, manager->NextInTreeOrder(node2_accessible));
  EXPECT_EQ(node4_accessible, manager->NextInTreeOrder(node3_accessible));
  EXPECT_EQ(node5_accessible, manager->NextInTreeOrder(node4_accessible));
  EXPECT_EQ(nullptr, manager->NextInTreeOrder(node5_accessible));

  EXPECT_EQ(nullptr, manager->PreviousInTreeOrder(nullptr, false));
  EXPECT_EQ(node4_accessible,
            manager->PreviousInTreeOrder(node5_accessible, false));
  EXPECT_EQ(node3_accessible,
            manager->PreviousInTreeOrder(node4_accessible, false));
  EXPECT_EQ(node2_accessible,
            manager->PreviousInTreeOrder(node3_accessible, false));
  EXPECT_EQ(root_accessible,
            manager->PreviousInTreeOrder(node2_accessible, false));

  EXPECT_EQ(nullptr, manager->PreviousInTreeOrder(nullptr, true));
  EXPECT_EQ(node4_accessible,
            manager->PreviousInTreeOrder(node5_accessible, true));
  EXPECT_EQ(node3_accessible,
            manager->PreviousInTreeOrder(node4_accessible, true));
  EXPECT_EQ(node2_accessible,
            manager->PreviousInTreeOrder(node3_accessible, true));
  EXPECT_EQ(root_accessible,
            manager->PreviousInTreeOrder(node2_accessible, true));

  EXPECT_EQ(ui::AX_TREE_ORDER_EQUAL,
            BrowserAccessibilityManager::CompareNodes(
                *root_accessible, *root_accessible));

  EXPECT_EQ(ui::AX_TREE_ORDER_BEFORE,
            BrowserAccessibilityManager::CompareNodes(
                *node2_accessible, *node3_accessible));
  EXPECT_EQ(ui::AX_TREE_ORDER_AFTER,
            BrowserAccessibilityManager::CompareNodes(
                *node3_accessible, *node2_accessible));

  EXPECT_EQ(ui::AX_TREE_ORDER_BEFORE,
            BrowserAccessibilityManager::CompareNodes(
                *node2_accessible, *node4_accessible));
  EXPECT_EQ(ui::AX_TREE_ORDER_AFTER,
            BrowserAccessibilityManager::CompareNodes(
                *node4_accessible, *node2_accessible));

  EXPECT_EQ(ui::AX_TREE_ORDER_BEFORE,
            BrowserAccessibilityManager::CompareNodes(
                *node3_accessible, *node4_accessible));
  EXPECT_EQ(ui::AX_TREE_ORDER_AFTER,
            BrowserAccessibilityManager::CompareNodes(
                *node4_accessible, *node3_accessible));

  EXPECT_EQ(ui::AX_TREE_ORDER_BEFORE,
            BrowserAccessibilityManager::CompareNodes(
                *root_accessible, *node2_accessible));
  EXPECT_EQ(ui::AX_TREE_ORDER_AFTER,
            BrowserAccessibilityManager::CompareNodes(
                *node2_accessible, *root_accessible));
}

TEST(BrowserAccessibilityManagerTest, TestNextPreviousTextOnlyObject) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;

  ui::AXNodeData node2;
  node2.id = 2;
  root.child_ids.push_back(2);

  ui::AXNodeData text1;
  text1.id = 3;
  text1.role = ui::AX_ROLE_STATIC_TEXT;
  root.child_ids.push_back(3);

  ui::AXNodeData node3;
  node3.id = 4;
  root.child_ids.push_back(4);

  ui::AXNodeData text2;
  text2.id = 5;
  text2.role = ui::AX_ROLE_STATIC_TEXT;
  node3.child_ids.push_back(5);

  ui::AXNodeData node4;
  node4.id = 6;
  node3.child_ids.push_back(6);

  ui::AXNodeData text3;
  text3.id = 7;
  text3.role = ui::AX_ROLE_STATIC_TEXT;
  node3.child_ids.push_back(7);

  ui::AXNodeData node5;
  node5.id = 8;
  root.child_ids.push_back(8);

  ui::AXNodeData text4;
  text4.id = 9;
  text4.role = ui::AX_ROLE_LINE_BREAK;
  node5.child_ids.push_back(9);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, node2, node3, node4, node5, text1, text2,
                           text3, text4),
          nullptr, new CountedBrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(4U, root_accessible->PlatformChildCount());
  BrowserAccessibility* node2_accessible = root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, node2_accessible);
  BrowserAccessibility* text1_accessible = root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, text1_accessible);
  BrowserAccessibility* node3_accessible = root_accessible->PlatformGetChild(2);
  ASSERT_NE(nullptr, node3_accessible);
  ASSERT_EQ(3U, node3_accessible->PlatformChildCount());
  BrowserAccessibility* text2_accessible =
      node3_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, text2_accessible);
  BrowserAccessibility* node4_accessible =
      node3_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, node4_accessible);
  BrowserAccessibility* text3_accessible =
      node3_accessible->PlatformGetChild(2);
  ASSERT_NE(nullptr, text3_accessible);
  BrowserAccessibility* node5_accessible = root_accessible->PlatformGetChild(3);
  ASSERT_NE(nullptr, node5_accessible);
  ASSERT_EQ(1U, node5_accessible->PlatformChildCount());
  BrowserAccessibility* text4_accessible =
      node5_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, text4_accessible);

  EXPECT_EQ(nullptr, manager->NextTextOnlyObject(nullptr));
  EXPECT_EQ(text1_accessible, manager->NextTextOnlyObject(root_accessible));
  EXPECT_EQ(text1_accessible, manager->NextTextOnlyObject(node2_accessible));
  EXPECT_EQ(text2_accessible, manager->NextTextOnlyObject(text1_accessible));
  EXPECT_EQ(text2_accessible, manager->NextTextOnlyObject(node3_accessible));
  EXPECT_EQ(text3_accessible, manager->NextTextOnlyObject(text2_accessible));
  EXPECT_EQ(text3_accessible, manager->NextTextOnlyObject(node4_accessible));
  EXPECT_EQ(text4_accessible, manager->NextTextOnlyObject(text3_accessible));
  EXPECT_EQ(text4_accessible, manager->NextTextOnlyObject(node5_accessible));
  EXPECT_EQ(nullptr, manager->NextTextOnlyObject(text4_accessible));

  EXPECT_EQ(nullptr, manager->PreviousTextOnlyObject(nullptr));
  EXPECT_EQ(
      text3_accessible, manager->PreviousTextOnlyObject(text4_accessible));
  EXPECT_EQ(
      text3_accessible, manager->PreviousTextOnlyObject(node5_accessible));
  EXPECT_EQ(
      text2_accessible, manager->PreviousTextOnlyObject(text3_accessible));
  EXPECT_EQ(
      text2_accessible, manager->PreviousTextOnlyObject(node4_accessible));
  EXPECT_EQ(
      text1_accessible, manager->PreviousTextOnlyObject(text2_accessible));
  EXPECT_EQ(
      text1_accessible, manager->PreviousTextOnlyObject(node3_accessible));
  EXPECT_EQ(nullptr, manager->PreviousTextOnlyObject(node2_accessible));
  EXPECT_EQ(nullptr, manager->PreviousTextOnlyObject(root_accessible));
}

TEST(BrowserAccessibilityManagerTest, TestFindIndicesInCommonParent) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;

  ui::AXNodeData div;
  div.id = 2;
  div.role = ui::AX_ROLE_GENERIC_CONTAINER;
  root.child_ids.push_back(div.id);

  ui::AXNodeData button;
  button.id = 3;
  button.role = ui::AX_ROLE_BUTTON;
  div.child_ids.push_back(button.id);

  ui::AXNodeData button_text;
  button_text.id = 4;
  button_text.role = ui::AX_ROLE_STATIC_TEXT;
  button_text.SetName("Button");
  button.child_ids.push_back(button_text.id);

  ui::AXNodeData line_break;
  line_break.id = 5;
  line_break.role = ui::AX_ROLE_LINE_BREAK;
  line_break.SetName("\n");
  div.child_ids.push_back(line_break.id);

  ui::AXNodeData paragraph;
  paragraph.id = 6;
  paragraph.role = ui::AX_ROLE_PARAGRAPH;
  root.child_ids.push_back(paragraph.id);

  ui::AXNodeData paragraph_text;
  paragraph_text.id = 7;
  paragraph_text.role = ui::AX_ROLE_STATIC_TEXT;
  paragraph.child_ids.push_back(paragraph_text.id);

  ui::AXNodeData paragraph_line1;
  paragraph_line1.id = 8;
  paragraph_line1.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  paragraph_line1.SetName("Hello ");
  paragraph_text.child_ids.push_back(paragraph_line1.id);

  ui::AXNodeData paragraph_line2;
  paragraph_line2.id = 9;
  paragraph_line2.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  paragraph_line2.SetName("world.");
  paragraph_text.child_ids.push_back(paragraph_line2.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, div, button, button_text, line_break,
                           paragraph, paragraph_text, paragraph_line1,
                           paragraph_line2),
          nullptr, new CountedBrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());
  BrowserAccessibility* div_accessible = root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, div_accessible);
  ASSERT_EQ(2U, div_accessible->PlatformChildCount());
  BrowserAccessibility* button_accessible = div_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, button_accessible);
  BrowserAccessibility* button_text_accessible =
      button_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, button_text_accessible);
  BrowserAccessibility* line_break_accessible =
      div_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, line_break_accessible);
  BrowserAccessibility* paragraph_accessible =
      root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, paragraph_accessible);
  BrowserAccessibility* paragraph_text_accessible =
      paragraph_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, paragraph_text_accessible);
  ASSERT_EQ(2U, paragraph_text_accessible->InternalChildCount());
  BrowserAccessibility* paragraph_line1_accessible =
      paragraph_text_accessible->InternalGetChild(0);
  ASSERT_NE(nullptr, paragraph_line1_accessible);
  BrowserAccessibility* paragraph_line2_accessible =
      paragraph_text_accessible->InternalGetChild(1);
  ASSERT_NE(nullptr, paragraph_line2_accessible);

  BrowserAccessibility* common_parent = nullptr;
  int child_index1, child_index2;
  EXPECT_FALSE(BrowserAccessibilityManager::FindIndicesInCommonParent(
      *root_accessible, *root_accessible, &common_parent, &child_index1,
      &child_index2));
  EXPECT_EQ(nullptr, common_parent);
  EXPECT_EQ(-1, child_index1);
  EXPECT_EQ(-1, child_index2);

  EXPECT_TRUE(BrowserAccessibilityManager::FindIndicesInCommonParent(
      *div_accessible, *paragraph_accessible, &common_parent, &child_index1,
      &child_index2));
  EXPECT_EQ(root_accessible, common_parent);
  EXPECT_EQ(0, child_index1);
  EXPECT_EQ(1, child_index2);

  EXPECT_TRUE(BrowserAccessibilityManager::FindIndicesInCommonParent(
      *div_accessible, *paragraph_line1_accessible, &common_parent,
      &child_index1, &child_index2));
  EXPECT_EQ(root_accessible, common_parent);
  EXPECT_EQ(0, child_index1);
  EXPECT_EQ(1, child_index2);

  EXPECT_TRUE(BrowserAccessibilityManager::FindIndicesInCommonParent(
      *line_break_accessible, *paragraph_text_accessible, &common_parent,
      &child_index1, &child_index2));
  EXPECT_EQ(root_accessible, common_parent);
  EXPECT_EQ(0, child_index1);
  EXPECT_EQ(1, child_index2);

  EXPECT_TRUE(BrowserAccessibilityManager::FindIndicesInCommonParent(
      *button_text_accessible, *line_break_accessible, &common_parent,
      &child_index1, &child_index2));
  EXPECT_EQ(div_accessible, common_parent);
  EXPECT_EQ(0, child_index1);
  EXPECT_EQ(1, child_index2);

  EXPECT_TRUE(BrowserAccessibilityManager::FindIndicesInCommonParent(
      *paragraph_accessible, *paragraph_line2_accessible, &common_parent,
      &child_index1, &child_index2));
  EXPECT_EQ(root_accessible, common_parent);
  EXPECT_EQ(1, child_index1);
  EXPECT_EQ(1, child_index2);

  EXPECT_TRUE(BrowserAccessibilityManager::FindIndicesInCommonParent(
      *paragraph_text_accessible, *paragraph_line1_accessible, &common_parent,
      &child_index1, &child_index2));
  EXPECT_EQ(paragraph_accessible, common_parent);
  EXPECT_EQ(0, child_index1);
  EXPECT_EQ(0, child_index2);

  EXPECT_TRUE(BrowserAccessibilityManager::FindIndicesInCommonParent(
      *paragraph_line1_accessible, *paragraph_line2_accessible, &common_parent,
      &child_index1, &child_index2));
  EXPECT_EQ(paragraph_text_accessible, common_parent);
  EXPECT_EQ(0, child_index1);
  EXPECT_EQ(1, child_index2);
}

TEST(BrowserAccessibilityManagerTest, TestGetTextForRange) {
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;

  ui::AXNodeData div;
  div.id = 2;
  div.role = ui::AX_ROLE_GENERIC_CONTAINER;
  root.child_ids.push_back(div.id);

  ui::AXNodeData button;
  button.id = 3;
  button.role = ui::AX_ROLE_BUTTON;
  div.child_ids.push_back(button.id);

  ui::AXNodeData button_text;
  button_text.id = 4;
  button_text.role = ui::AX_ROLE_STATIC_TEXT;
  button_text.SetName("Button");
  button.child_ids.push_back(button_text.id);

  ui::AXNodeData line_break;
  line_break.id = 5;
  line_break.role = ui::AX_ROLE_LINE_BREAK;
  line_break.SetName("\n");
  div.child_ids.push_back(line_break.id);

  ui::AXNodeData paragraph;
  paragraph.id = 6;
  paragraph.role = ui::AX_ROLE_PARAGRAPH;
  root.child_ids.push_back(paragraph.id);

  ui::AXNodeData paragraph_text;
  paragraph_text.id = 7;
  paragraph_text.role = ui::AX_ROLE_STATIC_TEXT;
  paragraph_text.SetName("Hello world.");
  paragraph.child_ids.push_back(paragraph_text.id);

  ui::AXNodeData paragraph_line1;
  paragraph_line1.id = 8;
  paragraph_line1.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  paragraph_line1.SetName("Hello ");
  paragraph_text.child_ids.push_back(paragraph_line1.id);

  ui::AXNodeData paragraph_line2;
  paragraph_line2.id = 9;
  paragraph_line2.role = ui::AX_ROLE_INLINE_TEXT_BOX;
  paragraph_line2.SetName("world.");
  paragraph_text.child_ids.push_back(paragraph_line2.id);

  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          MakeAXTreeUpdate(root, div, button, button_text, line_break,
                           paragraph, paragraph_text, paragraph_line1,
                           paragraph_line2),
          nullptr, new CountedBrowserAccessibilityFactory()));

  BrowserAccessibility* root_accessible = manager->GetRoot();
  ASSERT_NE(nullptr, root_accessible);
  ASSERT_EQ(2U, root_accessible->PlatformChildCount());
  BrowserAccessibility* div_accessible = root_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, div_accessible);
  ASSERT_EQ(2U, div_accessible->PlatformChildCount());
  BrowserAccessibility* button_accessible = div_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, button_accessible);
  BrowserAccessibility* button_text_accessible =
      button_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, button_text_accessible);
  BrowserAccessibility* line_break_accessible =
      div_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, line_break_accessible);
  BrowserAccessibility* paragraph_accessible =
      root_accessible->PlatformGetChild(1);
  ASSERT_NE(nullptr, paragraph_accessible);
  BrowserAccessibility* paragraph_text_accessible =
      paragraph_accessible->PlatformGetChild(0);
  ASSERT_NE(nullptr, paragraph_text_accessible);
  ASSERT_EQ(2U, paragraph_text_accessible->InternalChildCount());
  BrowserAccessibility* paragraph_line1_accessible =
      paragraph_text_accessible->InternalGetChild(0);
  ASSERT_NE(nullptr, paragraph_line1_accessible);
  BrowserAccessibility* paragraph_line2_accessible =
      paragraph_text_accessible->InternalGetChild(1);
  ASSERT_NE(nullptr, paragraph_line2_accessible);

  std::vector<const BrowserAccessibility*> text_only_objects =
      BrowserAccessibilityManager::FindTextOnlyObjectsInRange(*root_accessible,
                                                              *root_accessible);
  EXPECT_EQ(3U, text_only_objects.size());
  EXPECT_EQ(button_text_accessible, text_only_objects[0]);
  EXPECT_EQ(line_break_accessible, text_only_objects[1]);
  EXPECT_EQ(paragraph_text_accessible, text_only_objects[2]);

  text_only_objects = BrowserAccessibilityManager::FindTextOnlyObjectsInRange(
      *div_accessible, *paragraph_accessible);
  EXPECT_EQ(3U, text_only_objects.size());
  EXPECT_EQ(button_text_accessible, text_only_objects[0]);
  EXPECT_EQ(line_break_accessible, text_only_objects[1]);
  EXPECT_EQ(paragraph_text_accessible, text_only_objects[2]);

  EXPECT_EQ(base::ASCIIToUTF16("Button\nHello world."),
            BrowserAccessibilityManager::GetTextForRange(*root_accessible, 0,
                                                         *root_accessible, 19));
  EXPECT_EQ(base::ASCIIToUTF16("ton\nHello world."),
            BrowserAccessibilityManager::GetTextForRange(*root_accessible, 3,
                                                         *root_accessible, 19));
  EXPECT_EQ(base::ASCIIToUTF16("Button\nHello world."),
            BrowserAccessibilityManager::GetTextForRange(
                *div_accessible, 0, *paragraph_accessible, 12));
  EXPECT_EQ(base::ASCIIToUTF16("ton\nHello world."),
            BrowserAccessibilityManager::GetTextForRange(
                *div_accessible, 3, *paragraph_accessible, 12));

  EXPECT_EQ(base::ASCIIToUTF16("Button\n"),
            BrowserAccessibilityManager::GetTextForRange(*div_accessible, 0,
                                                         *div_accessible, 1));
  EXPECT_EQ(base::ASCIIToUTF16("Button\n"),
            BrowserAccessibilityManager::GetTextForRange(
                *button_accessible, 0, *line_break_accessible, 1));

  EXPECT_EQ(base::ASCIIToUTF16("Hello world."),
            BrowserAccessibilityManager::GetTextForRange(
                *paragraph_accessible, 0, *paragraph_accessible, 12));
  EXPECT_EQ(base::ASCIIToUTF16("Hello wor"),
            BrowserAccessibilityManager::GetTextForRange(
                *paragraph_accessible, 0, *paragraph_accessible, 9));
  EXPECT_EQ(base::ASCIIToUTF16("Hello world."),
            BrowserAccessibilityManager::GetTextForRange(
                *paragraph_text_accessible, 0, *paragraph_text_accessible, 12));
  EXPECT_EQ(base::ASCIIToUTF16(" world."),
            BrowserAccessibilityManager::GetTextForRange(
                *paragraph_text_accessible, 5, *paragraph_text_accessible, 12));
  EXPECT_EQ(base::ASCIIToUTF16("Hello world."),
            BrowserAccessibilityManager::GetTextForRange(
                *paragraph_accessible, 0, *paragraph_text_accessible, 12));
  EXPECT_EQ(
      base::ASCIIToUTF16("Hello "),
      BrowserAccessibilityManager::GetTextForRange(
          *paragraph_line1_accessible, 0, *paragraph_line1_accessible, 6));
  EXPECT_EQ(
      base::ASCIIToUTF16("Hello"),
      BrowserAccessibilityManager::GetTextForRange(
          *paragraph_line1_accessible, 0, *paragraph_line1_accessible, 5));
  EXPECT_EQ(
      base::ASCIIToUTF16("ello "),
      BrowserAccessibilityManager::GetTextForRange(
          *paragraph_line1_accessible, 1, *paragraph_line1_accessible, 6));
  EXPECT_EQ(
      base::ASCIIToUTF16("world."),
      BrowserAccessibilityManager::GetTextForRange(
          *paragraph_line2_accessible, 0, *paragraph_line2_accessible, 6));
  EXPECT_EQ(
      base::ASCIIToUTF16("orld"),
      BrowserAccessibilityManager::GetTextForRange(
          *paragraph_line2_accessible, 1, *paragraph_line2_accessible, 5));
  EXPECT_EQ(
      base::ASCIIToUTF16("Hello world."),
      BrowserAccessibilityManager::GetTextForRange(
          *paragraph_line1_accessible, 0, *paragraph_line2_accessible, 6));
  // Start and end positions could be reversed.
  EXPECT_EQ(
      base::ASCIIToUTF16("Hello world."),
      BrowserAccessibilityManager::GetTextForRange(
          *paragraph_line2_accessible, 6, *paragraph_line1_accessible, 0));
}

TEST(BrowserAccessibilityManagerTest, DeletingFocusedNodeDoesNotCrash) {
  // Create a really simple tree with one root node and one focused child.
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.child_ids.push_back(2);

  ui::AXNodeData node2;
  node2.id = 2;

  ui::AXTreeUpdate initial_state = MakeAXTreeUpdate(root, node2);
  initial_state.has_tree_data = true;
  initial_state.tree_data.focus_id = 2;
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          initial_state, nullptr, new CountedBrowserAccessibilityFactory()));

  ASSERT_EQ(1, manager->GetRoot()->GetId());
  ASSERT_EQ(2, manager->GetFocus()->GetId());

  // Now replace the tree with a new tree consisting of a single root.
  ui::AXNodeData root2;
  root2.id = 3;
  root2.role = ui::AX_ROLE_ROOT_WEB_AREA;

  std::vector<AXEventNotificationDetails> events2;
  events2.push_back(AXEventNotificationDetails());
  events2[0].update = MakeAXTreeUpdate(root2);
  events2[0].id = -1;
  events2[0].event_type = ui::AX_EVENT_NONE;
  manager->OnAccessibilityEvents(events2);

  // Make sure that the focused node was updated to the new root and
  // that this doesn't crash.
  ASSERT_EQ(3, manager->GetRoot()->GetId());
  ASSERT_EQ(3, manager->GetFocus()->GetId());
}

TEST(BrowserAccessibilityManagerTest, DeletingFocusedNodeDoesNotCrash2) {
  // Create a really simple tree with one root node and one focused child.
  ui::AXNodeData root;
  root.id = 1;
  root.role = ui::AX_ROLE_ROOT_WEB_AREA;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);
  root.child_ids.push_back(4);

  ui::AXNodeData node2;
  node2.id = 2;

  ui::AXNodeData node3;
  node3.id = 3;

  ui::AXNodeData node4;
  node4.id = 4;

  ui::AXTreeUpdate initial_state = MakeAXTreeUpdate(root, node2, node3, node4);
  initial_state.has_tree_data = true;
  initial_state.tree_data.focus_id = 2;
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          initial_state, nullptr, new CountedBrowserAccessibilityFactory()));

  ASSERT_EQ(1, manager->GetRoot()->GetId());
  ASSERT_EQ(2, manager->GetFocus()->GetId());

  // Now replace the tree with a new tree consisting of a single root.
  ui::AXNodeData root2;
  root2.id = 3;
  root2.role = ui::AX_ROLE_ROOT_WEB_AREA;

  // Make an update the explicitly clears the previous root.
  std::vector<AXEventNotificationDetails> events2;
  events2.push_back(AXEventNotificationDetails());
  events2[0].update = MakeAXTreeUpdate(root2);
  events2[0].update.node_id_to_clear = 1;
  events2[0].id = -1;
  events2[0].event_type = ui::AX_EVENT_NONE;
  manager->OnAccessibilityEvents(events2);

  // Make sure that the focused node was updated to the new root and
  // that this doesn't crash.
  ASSERT_EQ(3, manager->GetRoot()->GetId());
  ASSERT_EQ(3, manager->GetFocus()->GetId());
}

}  // namespace content
