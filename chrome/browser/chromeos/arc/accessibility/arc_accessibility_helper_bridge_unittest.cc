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
#include "components/exo/wm_helper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
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
    double GetDefaultDeviceScaleFactor() const override { return 1.0; }
    bool AreVerifiedSyncTokensNeeded() const override { return false; }

    DISALLOW_COPY_AND_ASSIGN(FakeWMHelper);
  };

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

  ArcAccessibilityHelperBridgeTest() = default;

  void SetUp() override {
    wm_helper_ = std::make_unique<FakeWMHelper>();
    exo::WMHelper::SetInstance(wm_helper_.get());
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
    exo::WMHelper::SetInstance(nullptr);
    wm_helper_.reset();
  }

  TestArcAccessibilityHelperBridge* accessibility_helper_bridge() {
    return accessibility_helper_bridge_.get();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<FakeWMHelper> wm_helper_;
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

}  // namespace arc
