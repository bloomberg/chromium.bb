// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include "ash/public/cpp/accessibility_types.h"
#include "ash/shell.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_accessibility_helper_instance.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace arc {

class ArcAccessibilityHelperBridgeBrowserTest : public InProcessBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    fake_accessibility_helper_instance_ =
        std::make_unique<FakeAccessibilityHelperInstance>();
    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->accessibility_helper()
        ->SetInstance(fake_accessibility_helper_instance_.get());
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->accessibility_helper());

    chromeos::AccessibilityManager::Get()->SetProfileForTest(
        browser()->profile());

    wm_helper_ = std::make_unique<exo::WMHelper>();
    exo::WMHelper::SetInstance(wm_helper_.get());
  }

  void TearDownOnMainThread() override {
    exo::WMHelper::SetInstance(nullptr);
    wm_helper_.reset();

    ArcServiceManager::Get()
        ->arc_bridge_service()
        ->accessibility_helper()
        ->CloseInstance(fake_accessibility_helper_instance_.get());
    fake_accessibility_helper_instance_.reset();
  }

 protected:
  std::unique_ptr<FakeAccessibilityHelperInstance>
      fake_accessibility_helper_instance_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
};

IN_PROC_BROWSER_TEST_F(ArcAccessibilityHelperBridgeBrowserTest,
                       PreferenceChange) {
  ASSERT_EQ(mojom::AccessibilityFilterType::OFF,
            fake_accessibility_helper_instance_->filter_type());

  exo::test::ExoTestHelper exo_test_helper;
  exo::test::ExoTestWindow test_window_1 =
      exo_test_helper.CreateWindow(640, 480, false /* is_modal */);
  exo::test::ExoTestWindow test_window_2 =
      exo_test_helper.CreateWindow(640, 480, false /* is_modal */);

  wm::ActivationClient* activation_client =
      ash::Shell::Get()->activation_client();
  activation_client->ActivateWindow(
      test_window_1.shell_surface()->GetWidget()->GetNativeWindow());
  ASSERT_EQ(test_window_1.shell_surface()->GetWidget()->GetNativeWindow(),
            activation_client->GetActiveWindow());
  ASSERT_FALSE(
      test_window_1.shell_surface()
          ->GetWidget()
          ->GetNativeWindow()
          ->GetProperty(
              aura::client::kAccessibilityTouchExplorationPassThrough));
  ASSERT_FALSE(
      test_window_2.shell_surface()
          ->GetWidget()
          ->GetNativeWindow()
          ->GetProperty(
              aura::client::kAccessibilityTouchExplorationPassThrough));

  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(
      true /* enable */, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_EQ(mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME,
            fake_accessibility_helper_instance_->filter_type());

  // Touch exploration pass through of test_window_1 (current active window)
  // would become true as no accessibility tree is available for it.
  EXPECT_TRUE(test_window_1.shell_surface()
                  ->GetWidget()
                  ->GetNativeWindow()
                  ->GetProperty(
                      aura::client::kAccessibilityTouchExplorationPassThrough));

  // Activate test_window_2 and confirm that it also becomes true.
  activation_client->ActivateWindow(
      test_window_2.shell_surface()->GetWidget()->GetNativeWindow());
  ASSERT_EQ(test_window_2.shell_surface()->GetWidget()->GetNativeWindow(),
            activation_client->GetActiveWindow());
  EXPECT_TRUE(test_window_2.shell_surface()
                  ->GetWidget()
                  ->GetNativeWindow()
                  ->GetProperty(
                      aura::client::kAccessibilityTouchExplorationPassThrough));
}

}  // namespace arc
