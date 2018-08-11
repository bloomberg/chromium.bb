// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"

#include "ash/shell.h"
#include "base/feature_list.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_features.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_accessibility_helper_instance.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "components/viz/common/features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
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

    wm_helper_ = std::make_unique<exo::WMHelper>(ash::Shell::Get()->aura_env());
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
  // TODO(penghuang): Re-enable once the EXO+Viz work is done and Arc can be
  // supported. https://crbug.com/807465
  if (base::FeatureList::IsEnabled(features::kVizDisplayCompositor))
    return;

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

  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(true);

  // Confirm that filter type is updated with preference change.
  EXPECT_EQ(
      base::FeatureList::IsEnabled(chromeos::features::kChromeVoxArcSupport)
          ? mojom::AccessibilityFilterType::ALL
          : mojom::AccessibilityFilterType::WHITELISTED_PACKAGE_NAME,
      fake_accessibility_helper_instance_->filter_type());

  // Touch exploration pass through of test_window_1 (current active window)
  // would become true as no accessibility tree is available for it. Note that
  // this value should be set to true in this test even for filter type ALL case
  // as we are not a dispatching accessibility event in this test case.
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
