// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <memory>
#include <unordered_map>
#include <utility>

#include "base/command_line.h"
#include "base/observer_list.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/accessibility_helper.mojom.h"
#include "components/exo/shell_surface.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/arc/notification/arc_notification_surface.h"
#include "ui/arc/notification/arc_notification_surface_manager.h"
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

  class ArcNotificationSurfaceManagerTest
      : public ArcNotificationSurfaceManager {
   public:
    void AddObserver(Observer* observer) override {
      observers_.AddObserver(observer);
    };

    void RemoveObserver(Observer* observer) override {
      observers_.RemoveObserver(observer);
    };

    ArcNotificationSurface* GetArcSurface(
        const std::string& notification_key) const override {
      auto it = surfaces_.find(notification_key);
      if (it == surfaces_.end())
        return nullptr;

      return it->second;
    }

    void AddSurface(ArcNotificationSurface* surface) {
      surfaces_[surface->GetNotificationKey()] = surface;

      for (auto& observer : observers_) {
        observer.OnNotificationSurfaceAdded(surface);
      }
    }

    void RemoveSurface(ArcNotificationSurface* surface) {
      surfaces_.erase(surface->GetNotificationKey());

      for (auto& observer : observers_) {
        observer.OnNotificationSurfaceRemoved(surface);
      }
    }

   private:
    std::map<std::string, ArcNotificationSurface*> surfaces_;
    base::ObserverList<Observer> observers_;
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
    arc_notification_surface_manager_ =
        std::make_unique<ArcNotificationSurfaceManagerTest>();
    accessibility_helper_bridge_ =
        std::make_unique<TestArcAccessibilityHelperBridge>(
            testing_profile_.get(), bridge_service_.get());
  }

  void TearDown() override {
    accessibility_helper_bridge_->Shutdown();
    accessibility_helper_bridge_.reset();
    arc_notification_surface_manager_.reset();
    bridge_service_.reset();
    testing_profile_.reset();
  }

  TestArcAccessibilityHelperBridge* accessibility_helper_bridge() {
    return accessibility_helper_bridge_.get();
  }

 protected:
  std::unique_ptr<ArcNotificationSurfaceManagerTest>
      arc_notification_surface_manager_;

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
//
// mojo: notification 1 created
// wayland: surface 1 added
// mojo: notification 1 removed
// mojo: notification 2 created
// wayland: surface 1 removed
// wayland: surface 2 added
// mojo: notification 2 removed
// wayland: surface 2 removed
TEST_F(ArcAccessibilityHelperBridgeTest, NotificationEventArriveFirst) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableChromeVoxArcSupport);

  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  arc_notification_surface_manager_->AddObserver(helper_bridge);

  const auto& notification_key_to_tree_ =
      helper_bridge->notification_key_to_tree_for_test();
  ASSERT_EQ(0U, notification_key_to_tree_.size());

  // mojo: notification 1 created
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);
  auto event1 = arc::mojom::AccessibilityEventData::New();
  event1->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event1->notification_key = base::make_optional<std::string>(kNotificationKey);
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event1.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // wayland: surface 1 added
  ArcNotificationSurfaceTest test_surface(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface);

  // Confirm that axtree id is set to the surface.
  auto it = notification_key_to_tree_.find(kNotificationKey);
  EXPECT_NE(notification_key_to_tree_.end(), it);
  AXTreeSourceArc* tree = it->second.get();
  ui::AXTreeData tree_data;
  tree->GetTreeData(&tree_data);
  EXPECT_EQ(tree_data.tree_id, test_surface.GetAXTreeId());

  // mojo: notification 1 removed
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED);

  // Ax tree of the surface should be reset as the tree no longer exists.
  EXPECT_EQ(-1, test_surface.GetAXTreeId());

  EXPECT_EQ(0U, notification_key_to_tree_.size());

  // mojo: notification 2 created
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);
  auto event3 = arc::mojom::AccessibilityEventData::New();
  event3->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event3->notification_key = base::make_optional<std::string>(kNotificationKey);
  event3->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event3.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // Ax tree from the second event is attached to the first surface. This is
  // expected behavior.
  auto it2 = notification_key_to_tree_.find(kNotificationKey);
  EXPECT_NE(notification_key_to_tree_.end(), it2);
  AXTreeSourceArc* tree2 = it2->second.get();
  ui::AXTreeData tree_data2;
  tree2->GetTreeData(&tree_data2);
  EXPECT_EQ(tree_data2.tree_id, test_surface.GetAXTreeId());

  // wayland: surface 1 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface);

  // Tree shouldn't be removed as a surface for the second one will come.
  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // wayland: surface 2 added
  ArcNotificationSurfaceTest test_surface_2(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface_2);

  EXPECT_EQ(tree_data2.tree_id, test_surface_2.GetAXTreeId());

  // mojo: notification 2 removed
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED);

  EXPECT_EQ(0U, notification_key_to_tree_.size());

  // wayland: surface 2 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface_2);
}

// This is the case for testing backward compatibility with creating
// notification by WINDOW_STATE_CHANGED event.
//
// mojo: notification 1 created
// wayland: surface 1 created
// mojo: notification 2 created
// wayland: surface 1 removed
// wayland: surface 2 created
// wayland: surface 2 removed
//
// wayland: surface 3 created
// mojo: notification 3 created
// wayland: surface 3 removed
TEST_F(ArcAccessibilityHelperBridgeTest, NotificationBackwardCompat) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableChromeVoxArcSupport);

  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  arc_notification_surface_manager_->AddObserver(helper_bridge);

  const auto& notification_key_to_tree_ =
      helper_bridge->notification_key_to_tree_for_test();
  ASSERT_EQ(0U, notification_key_to_tree_.size());

  // mojo: notification 1 created
  auto event1 = arc::mojom::AccessibilityEventData::New();
  event1->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event1->notification_key = base::make_optional<std::string>(kNotificationKey);
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event1.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // wayland: surface 1 added
  ArcNotificationSurfaceTest test_surface1(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface1);

  auto it = notification_key_to_tree_.find(kNotificationKey);
  EXPECT_NE(notification_key_to_tree_.end(), it);
  AXTreeSourceArc* tree = it->second.get();
  ui::AXTreeData tree_data;
  tree->GetTreeData(&tree_data);
  EXPECT_EQ(tree_data.tree_id, test_surface1.GetAXTreeId());

  // mojo: notification 2 created
  auto event2 = arc::mojom::AccessibilityEventData::New();
  event2->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event2->notification_key = base::make_optional<std::string>(kNotificationKey);
  event2->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event2.Clone());

  auto it2 = notification_key_to_tree_.find(kNotificationKey);
  EXPECT_NE(notification_key_to_tree_.end(), it);
  AXTreeSourceArc* tree2 = it2->second.get();
  ui::AXTreeData tree_data2;
  tree2->GetTreeData(&tree_data2);
  EXPECT_EQ(tree_data2.tree_id, test_surface1.GetAXTreeId());

  // wayland: surface 1 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface1);

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // wayland: surface 2 added
  ArcNotificationSurfaceTest test_surface2(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface2);

  EXPECT_EQ(tree_data2.tree_id, test_surface2.GetAXTreeId());

  // wayland: surface 2 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface2);

  EXPECT_EQ(0U, notification_key_to_tree_.size());

  // wayland: surface 3 added
  ArcNotificationSurfaceTest test_surface3(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface3);

  // mojo: notification 3 created
  auto event3 = arc::mojom::AccessibilityEventData::New();
  event3->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event3->notification_key = base::make_optional<std::string>(kNotificationKey);
  event3->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event3.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  auto it3 = notification_key_to_tree_.find(kNotificationKey);
  EXPECT_NE(notification_key_to_tree_.end(), it3);
  AXTreeSourceArc* tree3 = it3->second.get();
  ui::AXTreeData tree_data3;
  tree3->GetTreeData(&tree_data3);
  EXPECT_EQ(tree_data3.tree_id, test_surface3.GetAXTreeId());

  // wayland: surface 3 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface3);

  EXPECT_EQ(0U, notification_key_to_tree_.size());
}

// This is the case where surface creation/removal arrive before mojo events.
//
// wayland: surface 1 added
// wayland: surface 1 removed
// mojo: notification 1 created
// mojo: notification 1 removed
TEST_F(ArcAccessibilityHelperBridgeTest, NotificationSurfaceArriveFirst) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableChromeVoxArcSupport);

  TestArcAccessibilityHelperBridge* helper_bridge =
      accessibility_helper_bridge();
  arc_notification_surface_manager_->AddObserver(helper_bridge);

  const auto& notification_key_to_tree_ =
      helper_bridge->notification_key_to_tree_for_test();
  ASSERT_EQ(0U, notification_key_to_tree_.size());

  // wayland: surface 1 added
  ArcNotificationSurfaceTest test_surface(kNotificationKey);
  arc_notification_surface_manager_->AddSurface(&test_surface);

  // wayland: surface 1 removed
  arc_notification_surface_manager_->RemoveSurface(&test_surface);

  // mojo: notification 1 created
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_CREATED);
  auto event1 = arc::mojom::AccessibilityEventData::New();
  event1->event_type = arc::mojom::AccessibilityEventType::WINDOW_STATE_CHANGED;
  event1->notification_key = base::make_optional<std::string>(kNotificationKey);
  event1->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  helper_bridge->OnAccessibilityEvent(event1.Clone());

  EXPECT_EQ(1U, notification_key_to_tree_.size());

  // mojo: notification 2 removed
  helper_bridge->OnNotificationStateChanged(
      kNotificationKey,
      arc::mojom::AccessibilityNotificationStateType::SURFACE_REMOVED);

  EXPECT_EQ(0U, notification_key_to_tree_.size());
}

}  // namespace arc
