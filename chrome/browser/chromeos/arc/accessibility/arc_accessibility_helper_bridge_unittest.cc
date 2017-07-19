// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include <unordered_map>
#include <utility>

#include "base/command_line.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/common/accessibility_helper.mojom.h"
#include "components/exo/wm_helper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"

namespace arc {

class ArcAccessibilityHelperBridgeTest : public testing::Test {
 public:
  class FakeWMHelper : public exo::WMHelper {
   public:
    FakeWMHelper() = default;

   private:
    const display::ManagedDisplayInfo& GetDisplayInfo(
        int64_t display_id) const override {
      static const display::ManagedDisplayInfo info;
      return info;
    }
    aura::Window* GetPrimaryDisplayContainer(int container_id) override {
      return nullptr;
    }
    aura::Window* GetActiveWindow() const override { return nullptr; }
    aura::Window* GetFocusedWindow() const override { return nullptr; }
    ui::CursorSize GetCursorSize() const override {
      return ui::CursorSize::kNormal;
    }
    const display::Display& GetCursorDisplay() const override {
      static const display::Display display;
      return display;
    }
    void AddPreTargetHandler(ui::EventHandler* handler) override {}
    void PrependPreTargetHandler(ui::EventHandler* handler) override {}
    void RemovePreTargetHandler(ui::EventHandler* handler) override {}
    void AddPostTargetHandler(ui::EventHandler* handler) override {}
    void RemovePostTargetHandler(ui::EventHandler* handler) override {}
    bool IsTabletModeWindowManagerEnabled() const override { return false; }

    DISALLOW_COPY_AND_ASSIGN(FakeWMHelper);
  };

  ArcAccessibilityHelperBridgeTest() = default;

  void SetUp() override {
    wm_helper_ = base::MakeUnique<FakeWMHelper>();
    exo::WMHelper::SetInstance(wm_helper_.get());
    testing_profile_ = base::MakeUnique<TestingProfile>();
    bridge_service_ = base::MakeUnique<ArcBridgeService>();
    accessibility_helper_bridge_ =
        base::MakeUnique<ArcAccessibilityHelperBridge>(testing_profile_.get(),
                                                       bridge_service_.get());
  }

  void TearDown() override {
    accessibility_helper_bridge_->Shutdown();
    accessibility_helper_bridge_.reset();
    bridge_service_.reset();
    testing_profile_.reset();
    exo::WMHelper::SetInstance(nullptr);
    wm_helper_.reset();
  }

  ArcAccessibilityHelperBridge* accessibility_helper_bridge() {
    return accessibility_helper_bridge_.get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<FakeWMHelper> wm_helper_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<ArcAccessibilityHelperBridge> accessibility_helper_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ArcAccessibilityHelperBridgeTest);
};

TEST_F(ArcAccessibilityHelperBridgeTest, TaskLifecycle) {
  ArcAccessibilityHelperBridge* helper_bridge = accessibility_helper_bridge();
  const auto& package_to_tree = helper_bridge->package_name_to_tree_for_test();
  const auto& package_to_ids =
      helper_bridge->package_name_to_task_ids_for_test();

  ASSERT_EQ(0U, package_to_tree.size());
  ASSERT_EQ(0U, package_to_ids.size());

  helper_bridge->OnTaskCreated(1, "com.android.vending", "launch", "");
  ASSERT_EQ(0U, package_to_tree.size());
  ASSERT_EQ(1U, package_to_ids.size());
  auto it1 = package_to_ids.find("com.android.vending");
  ASSERT_NE(package_to_ids.end(), it1);
  ASSERT_EQ(1U, it1->second.size());
  ASSERT_EQ(1U, it1->second.count(1));

  helper_bridge->OnTaskCreated(2, "com.android.vending", "app", "");
  ASSERT_EQ(0U, package_to_tree.size());
  ASSERT_EQ(1U, package_to_ids.size());
  auto it2 = package_to_ids.find("com.android.vending");
  ASSERT_NE(package_to_ids.end(), it2);
  ASSERT_EQ(2U, it2->second.size());
  ASSERT_EQ(1U, it2->second.count(1));
  ASSERT_EQ(1U, it2->second.count(2));

  helper_bridge->OnTaskCreated(3, "com.android.music", "app", "");
  ASSERT_EQ(0U, package_to_tree.size());
  ASSERT_EQ(2U, package_to_ids.size());
  auto it3 = package_to_ids.find("com.android.music");
  ASSERT_NE(package_to_ids.end(), it3);
  ASSERT_EQ(1U, it3->second.size());
  ASSERT_EQ(1U, it3->second.count(3));

  helper_bridge->OnTaskDestroyed(1);
  ASSERT_EQ(0U, package_to_tree.size());
  ASSERT_EQ(2U, package_to_ids.size());
  it1 = package_to_ids.find("com.android.vending");
  ASSERT_NE(package_to_ids.end(), it1);
  ASSERT_EQ(1U, it1->second.size());
  ASSERT_EQ(0U, it1->second.count(1));
  ASSERT_EQ(1U, it1->second.count(2));

  helper_bridge->OnTaskDestroyed(2);
  ASSERT_EQ(0U, package_to_tree.size());
  ASSERT_EQ(1U, package_to_ids.size());
  it2 = package_to_ids.find("com.android.vending");
  ASSERT_EQ(package_to_ids.end(), it2);

  helper_bridge->OnTaskDestroyed(3);
  ASSERT_EQ(0U, package_to_tree.size());
  ASSERT_EQ(0U, package_to_ids.size());
}

TEST_F(ArcAccessibilityHelperBridgeTest, AXTreeSourceLifetime) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableChromeVoxArcSupport);

  ArcAccessibilityHelperBridge* helper_bridge = accessibility_helper_bridge();
  const auto& package_to_tree = helper_bridge->package_name_to_tree_for_test();
  const auto& package_to_ids =
      helper_bridge->package_name_to_task_ids_for_test();

  ASSERT_EQ(0U, package_to_tree.size());
  ASSERT_EQ(0U, package_to_ids.size());

  helper_bridge->OnTaskCreated(1, "com.android.vending", "launch", "");
  ASSERT_EQ(0U, package_to_tree.size());
  ASSERT_EQ(1U, package_to_ids.size());
  auto it1 = package_to_ids.find("com.android.vending");
  ASSERT_NE(package_to_ids.end(), it1);
  ASSERT_EQ(1U, it1->second.size());
  ASSERT_EQ(1U, it1->second.count(1));

  auto event = arc::mojom::AccessibilityEventData::New();
  event->source_id = 1;
  event->event_type = arc::mojom::AccessibilityEventType::VIEW_FOCUSED;
  event->node_data.push_back(arc::mojom::AccessibilityNodeInfoData::New());
  event->node_data[0]->id = 1;
  event->node_data[0]->string_properties =
      std::unordered_map<arc::mojom::AccessibilityStringProperty,
                         std::string>();
  event->node_data[0]->string_properties.value().insert(
      std::make_pair(arc::mojom::AccessibilityStringProperty::PACKAGE_NAME,
                     "com.android.vending"));

  // Task 1 is not current; gets rejected.
  helper_bridge->OnAccessibilityEvent(event.Clone());
  ASSERT_EQ(0U, package_to_tree.size());

  // Task 1 is now current.
  helper_bridge->OnTaskSetActive(1);
  helper_bridge->OnAccessibilityEvent(event.Clone());
  ASSERT_EQ(1U, package_to_tree.size());
}

}  // namespace arc
