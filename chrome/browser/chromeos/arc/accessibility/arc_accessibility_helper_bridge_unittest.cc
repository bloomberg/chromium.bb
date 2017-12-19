// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <memory>
#include <unordered_map>
#include <utility>

#include "base/command_line.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/accessibility_helper.mojom.h"
#include "components/exo/shell_surface.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/arc/notification/arc_notification_surface.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"

namespace arc {

namespace {

const char kNotificationKey[] = "unit.test.notification";

}  // namespace

class ArcAccessibilityHelperBridgeTest : public testing::Test {
 public:
  class TestArcAccessibilityHelperBridge : public ArcAccessibilityHelperBridge {
   public:
    TestArcAccessibilityHelperBridge(content::BrowserContext* browser_context,
                                     ArcBridgeService* arc_bridge_service)
        : ArcAccessibilityHelperBridge(browser_context, arc_bridge_service),
          window_(new aura::Window(nullptr)) {
      window_->Init(ui::LAYER_NOT_DRAWN);
    }

    void SetActiveWindowId(const std::string& id) {
      exo::ShellSurface::SetApplicationId(window_.get(), id);
    }

   protected:
    aura::Window* GetActiveWindow() override { return window_.get(); }

   private:
    std::unique_ptr<aura::Window> window_;

    DISALLOW_COPY_AND_ASSIGN(TestArcAccessibilityHelperBridge);
  };

  class ArcNotificationSurfaceTest : public ArcNotificationSurface {
   public:
    explicit ArcNotificationSurfaceTest(std::string notification_key)
        : notification_key_(notification_key), ax_tree_id_(-1) {}

    gfx::Size GetSize() const override { return gfx::Size(); }

    aura::Window* GetWindow() const override { return nullptr; }

    aura::Window* GetContentWindow() const override { return nullptr; }

    const std::string& GetNotificationKey() const override {
      return notification_key_;
    }

    void Attach(views::NativeViewHost* native_view_host) override {}

    void Detach() override {}

    bool IsAttached() const override { return false; }

    views::NativeViewHost* GetAttachedHost() const override { return nullptr; }

    void FocusSurfaceWindow() override {}

    void SetAXTreeId(int32_t ax_tree_id) override { ax_tree_id_ = ax_tree_id; }

    int32_t GetAXTreeId() const override { return ax_tree_id_; }

   private:
    const std::string notification_key_;
    int32_t ax_tree_id_;
  };

  ArcAccessibilityHelperBridgeTest() = default;

  void SetUp() override {
    testing_profile_ = std::make_unique<TestingProfile>();
    bridge_service_ = std::make_unique<ArcBridgeService>();
    accessibility_helper_bridge_ =
        std::make_unique<TestArcAccessibilityHelperBridge>(
            testing_profile_.get(), bridge_service_.get());
  }

  void TearDown() override {
    accessibility_helper_bridge_->Shutdown();
    accessibility_helper_bridge_.reset();
    bridge_service_.reset();
    testing_profile_.reset();
  }

  TestArcAccessibilityHelperBridge* accessibility_helper_bridge() {
    return accessibility_helper_bridge_.get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<TestArcAccessibilityHelperBridge>
      accessibility_helper_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ArcAccessibilityHelperBridgeTest);
};

TEST_F(ArcAccessibilityHelperBridgeTest, TaskAndAXTreeLifecycle) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableChromeVoxArcSupport);

  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  const auto& task_id_to_tree = helper_bridge->task_id_to_tree_for_test();
  ASSERT_EQ(0U, task_id_to_tree.size());

  auto event1 = arc::mojom::AccessibilityEventData::New();
  event1->source_id = 1;
  event1->task_id = 1;
  event1->event_type = arc::mojom::AccessibilityEventType::VIEW_FOCUSED;
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event1->node_data[0]->id = 1;
  event1->node_data[0]->string_properties =
      std::unordered_map<arc::mojom::AccessibilityStringProperty,
                         std::string>();
  event1->node_data[0]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::PACKAGE_NAME,
                     "com.android.vending"));

  // There's no active window.
  helper_bridge->OnAccessibilityEvent(event1.Clone());
  ASSERT_EQ(0U, task_id_to_tree.size());

  // Let's make task 1 active by activating the window.
  helper_bridge->SetActiveWindowId(std::string("org.chromium.arc.1"));
  helper_bridge->OnAccessibilityEvent(event1.Clone());
  ASSERT_EQ(1U, task_id_to_tree.size());

  // Same package name, different task.
  auto event2 = arc::mojom::AccessibilityEventData::New();
  event2->source_id = 2;
  event2->task_id = 2;
  event2->event_type = arc::mojom::AccessibilityEventType::VIEW_FOCUSED;
  event2->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event2->node_data[0]->id = 2;
  event2->node_data[0]->string_properties =
      std::unordered_map<arc::mojom::AccessibilityStringProperty,
                         std::string>();
  event2->node_data[0]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::PACKAGE_NAME,
                     "com.android.vending"));

  // Active window is still task 1.
  helper_bridge->OnAccessibilityEvent(event2.Clone());
  ASSERT_EQ(1U, task_id_to_tree.size());

  // Now make task 2 active.
  helper_bridge->SetActiveWindowId(std::string("org.chromium.arc.2"));
  helper_bridge->OnAccessibilityEvent(event2.Clone());
  ASSERT_EQ(2U, task_id_to_tree.size());

  // Same task id, different package name.
  event2->node_data.clear();
  event2->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event2->node_data[0]->id = 3;
  event2->node_data[0]->string_properties =
      std::unordered_map<arc::mojom::AccessibilityStringProperty,
                         std::string>();
  event2->node_data[0]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::PACKAGE_NAME,
                     "com.google.music"));

  // No new tasks tree mappings should have occurred.
  helper_bridge->OnAccessibilityEvent(event2.Clone());
  ASSERT_EQ(2U, task_id_to_tree.size());

  helper_bridge->OnTaskDestroyed(1);
  ASSERT_EQ(1U, task_id_to_tree.size());

  helper_bridge->OnTaskDestroyed(2);
  ASSERT_EQ(0U, task_id_to_tree.size());
}

// Accessibility event and surface creation/removal are sent in different
// channels, mojo and wayland. Order of those events can be changed. This is the
// case where mojo events arrive earlier than surface creation/removal.
TEST_F(ArcAccessibilityHelperBridgeTest, NotificationEventArriveFirst) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableChromeVoxArcSupport);

  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();

  const auto& notification_key_to_tree_ =
      helper_bridge->notification_key_to_tree_for_test();
  ASSERT_EQ(0U, notification_key_to_tree_.size());

  // Dispatch an accessibility event for creation of notification.
  auto event1 = arc::mojom::AccessibilityEventData::New();
  event1->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event1->notification_key = base::make_optional<std::string>(kNotificationKey);
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event1.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // Add notification surface for the first event.
  ArcNotificationSurfaceTest test_surface(kNotificationKey);
  helper_bridge->OnNotificationSurfaceAdded(&test_surface);

  // Confirm that axtree id is set to the surface.
  auto it = notification_key_to_tree_.find(kNotificationKey);
  EXPECT_NE(notification_key_to_tree_.end(), it);
  AXTreeSourceArc* tree = it->second->tree.get();
  ui::AXTreeData tree_data;
  tree->GetTreeData(&tree_data);
  EXPECT_EQ(tree_data.tree_id, test_surface.GetAXTreeId());

  // Dispatch second event for notification creation before surface is removed.
  auto event2 = arc::mojom::AccessibilityEventData::New();
  event2->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event2->notification_key = base::make_optional<std::string>(kNotificationKey);
  event2->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event2.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // Remove notification surface for the first event.
  helper_bridge->OnNotificationSurfaceRemoved(&test_surface);

  // Tree shouldn't be removed as a surface for the second event will come.
  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // Add notification surface for the second event, and confirm that axtree id
  // is set.
  ArcNotificationSurfaceTest test_surface_2(kNotificationKey);
  helper_bridge->OnNotificationSurfaceAdded(&test_surface_2);
  EXPECT_EQ(tree_data.tree_id, test_surface_2.GetAXTreeId());

  // Remove notification surface for the second event, and confirm that tree is
  // deleted.
  helper_bridge->OnNotificationSurfaceRemoved(&test_surface_2);
  EXPECT_EQ(0U, notification_key_to_tree_.size());
}

}  // namespace arc
