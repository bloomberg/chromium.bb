// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

// Creates an array with |num_elements| handlers and makes |chrome_index|-th
// handler "Chrome". If Chrome is not necessary, set |chrome_index| to
// |num_elements|.
std::vector<mojom::IntentHandlerInfoPtr> CreateArray(size_t num_elements,
                                                     size_t chrome_index) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers;
  for (size_t i = 0; i < num_elements; ++i) {
    mojom::IntentHandlerInfoPtr handler = mojom::IntentHandlerInfo::New();
    handler->name = "Name";
    if (i == chrome_index) {
      handler->package_name =
          ArcIntentHelperBridge::kArcIntentHelperPackageName;
    } else {
      handler->package_name = "com.package";
    }
    handlers.push_back(std::move(handler));
  }
  return handlers;
}

}  // namespace

TEST(ArcNavigationThrottleTest, TestIsAppAvailable) {
  // Test an empty array.
  EXPECT_FALSE(
      ArcNavigationThrottle::IsAppAvailableForTesting(CreateArray(0, 0)));
  // Chrome only.
  EXPECT_FALSE(
      ArcNavigationThrottle::IsAppAvailableForTesting(CreateArray(1, 0)));
  // Chrome and another app.
  EXPECT_TRUE(
      ArcNavigationThrottle::IsAppAvailableForTesting(CreateArray(2, 0)));
  EXPECT_TRUE(
      ArcNavigationThrottle::IsAppAvailableForTesting(CreateArray(2, 1)));
  // App(s) only. This doesn't happen on production though.
  EXPECT_TRUE(
      ArcNavigationThrottle::IsAppAvailableForTesting(CreateArray(1, 1)));
  EXPECT_TRUE(
      ArcNavigationThrottle::IsAppAvailableForTesting(CreateArray(2, 2)));
}

TEST(ArcNavigationThrottleTest, TestFindPreferredApp) {
  // Test an empty array.
  EXPECT_EQ(
      0u, ArcNavigationThrottle::FindPreferredAppForTesting(CreateArray(0, 0)));
  // Test no-preferred-app cases.
  EXPECT_EQ(
      1u, ArcNavigationThrottle::FindPreferredAppForTesting(CreateArray(1, 0)));
  EXPECT_EQ(
      2u, ArcNavigationThrottle::FindPreferredAppForTesting(CreateArray(2, 1)));
  EXPECT_EQ(
      3u, ArcNavigationThrottle::FindPreferredAppForTesting(CreateArray(3, 2)));
  // Add a preferred app and call the function.
  for (size_t i = 0; i < 3; ++i) {
    std::vector<mojom::IntentHandlerInfoPtr> handlers = CreateArray(3, 0);
    handlers[i]->is_preferred = true;
    EXPECT_EQ(i, ArcNavigationThrottle::FindPreferredAppForTesting(handlers))
        << i;
  }
}

TEST(ArcNavigationThrottleTest, TestGetAppIndex) {
  const std::string package_name =
      ArcIntentHelperBridge::kArcIntentHelperPackageName;
  // Test an empty array.
  EXPECT_EQ(
      0u, ArcNavigationThrottle::GetAppIndex(CreateArray(0, 0), package_name));
  // Test Chrome-only case.
  EXPECT_EQ(
      0u, ArcNavigationThrottle::GetAppIndex(CreateArray(1, 0), package_name));
  // Test not-found cases.
  EXPECT_EQ(
      1u, ArcNavigationThrottle::GetAppIndex(CreateArray(1, 1), package_name));
  EXPECT_EQ(
      2u, ArcNavigationThrottle::GetAppIndex(CreateArray(2, 2), package_name));
  // Test other cases.
  EXPECT_EQ(
      0u, ArcNavigationThrottle::GetAppIndex(CreateArray(2, 0), package_name));
  EXPECT_EQ(
      1u, ArcNavigationThrottle::GetAppIndex(CreateArray(2, 1), package_name));
  EXPECT_EQ(
      0u, ArcNavigationThrottle::GetAppIndex(CreateArray(3, 0), package_name));
  EXPECT_EQ(
      1u, ArcNavigationThrottle::GetAppIndex(CreateArray(3, 1), package_name));
  EXPECT_EQ(
      2u, ArcNavigationThrottle::GetAppIndex(CreateArray(3, 2), package_name));
}

TEST(ArcNavigationThrottleTest, TestGetPickerAction) {
  // Expect PickerAction::ERROR if the close_reason is ERROR.
  EXPECT_EQ(ArcNavigationThrottle::PickerAction::ERROR,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::ERROR, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::ERROR,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::ERROR, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::ERROR,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::ERROR, false));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::ERROR,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::ERROR, false));

  // Expect PickerAction::DIALOG_DEACTIVATED if the close_reason is
  // DIALOG_DEACTIVATED.
  EXPECT_EQ(ArcNavigationThrottle::PickerAction::DIALOG_DEACTIVATED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::DIALOG_DEACTIVATED, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::DIALOG_DEACTIVATED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::DIALOG_DEACTIVATED, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::DIALOG_DEACTIVATED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::DIALOG_DEACTIVATED, false));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::DIALOG_DEACTIVATED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::DIALOG_DEACTIVATED, false));

  // Expect PickerAction::PREFERRED_ACTIVITY_FOUND if the close_reason is
  // PREFERRED_APP_FOUND.
  EXPECT_EQ(ArcNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::PREFERRED_APP_FOUND, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::PREFERRED_APP_FOUND, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::PREFERRED_APP_FOUND, false));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::PREFERRED_APP_FOUND, false));

  // Expect PREFERRED depending on the value of |should_persist|, and |app_type|
  // to be ignored if reason is STAY_IN_CHROME.
  EXPECT_EQ(ArcNavigationThrottle::PickerAction::CHROME_PREFERRED_PRESSED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::STAY_IN_CHROME, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::CHROME_PREFERRED_PRESSED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::STAY_IN_CHROME, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::CHROME_PRESSED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::STAY_IN_CHROME, false));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::CHROME_PRESSED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::STAY_IN_CHROME, false));

  // Expect PREFERRED depending on the value of |should_persist|, and
  // INVALID/ARC to be chosen if reason is OPEN_APP.
  EXPECT_EQ(ArcNavigationThrottle::PickerAction::INVALID,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::OPEN_APP, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::ARC_APP_PREFERRED_PRESSED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::OPEN_APP, true));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::INVALID,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::INVALID,
                chromeos::IntentPickerCloseReason::OPEN_APP, false));

  EXPECT_EQ(ArcNavigationThrottle::PickerAction::ARC_APP_PRESSED,
            ArcNavigationThrottle::GetPickerAction(
                chromeos::AppType::ARC,
                chromeos::IntentPickerCloseReason::OPEN_APP, false));
}

TEST(ArcNavigationThrottleTest, TestGetDestinationPlatform) {
  const std::string chrome_app =
      ArcIntentHelperBridge::kArcIntentHelperPackageName;
  const std::string non_chrome_app = "fake_package";

  // When the PickerAction is either ERROR or DIALOG_DEACTIVATED we MUST stay in
  // Chrome not taking into account the selected_app_package.
  EXPECT_EQ(ArcNavigationThrottle::Platform::CHROME,
            ArcNavigationThrottle::GetDestinationPlatform(
                chrome_app, ArcNavigationThrottle::PickerAction::ERROR));
  EXPECT_EQ(ArcNavigationThrottle::Platform::CHROME,
            ArcNavigationThrottle::GetDestinationPlatform(
                non_chrome_app, ArcNavigationThrottle::PickerAction::ERROR));
  EXPECT_EQ(
      ArcNavigationThrottle::Platform::CHROME,
      ArcNavigationThrottle::GetDestinationPlatform(
          chrome_app, ArcNavigationThrottle::PickerAction::DIALOG_DEACTIVATED));
  EXPECT_EQ(ArcNavigationThrottle::Platform::CHROME,
            ArcNavigationThrottle::GetDestinationPlatform(
                non_chrome_app,
                ArcNavigationThrottle::PickerAction::DIALOG_DEACTIVATED));

  // Under any other PickerAction, stay in Chrome only if the package is Chrome.
  // Otherwise redirect to ARC.
  EXPECT_EQ(ArcNavigationThrottle::Platform::CHROME,
            ArcNavigationThrottle::GetDestinationPlatform(
                chrome_app,
                ArcNavigationThrottle::PickerAction::OBSOLETE_ALWAYS_PRESSED));
  EXPECT_EQ(
      ArcNavigationThrottle::Platform::CHROME,
      ArcNavigationThrottle::GetDestinationPlatform(
          chrome_app,
          ArcNavigationThrottle::PickerAction::OBSOLETE_JUST_ONCE_PRESSED));
  EXPECT_EQ(ArcNavigationThrottle::Platform::CHROME,
            ArcNavigationThrottle::GetDestinationPlatform(
                chrome_app,
                ArcNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND));

  // Go to ARC on any other case.
  EXPECT_EQ(ArcNavigationThrottle::Platform::ARC,
            ArcNavigationThrottle::GetDestinationPlatform(
                non_chrome_app,
                ArcNavigationThrottle::PickerAction::OBSOLETE_ALWAYS_PRESSED));
  EXPECT_EQ(
      ArcNavigationThrottle::Platform::ARC,
      ArcNavigationThrottle::GetDestinationPlatform(
          non_chrome_app,
          ArcNavigationThrottle::PickerAction::OBSOLETE_JUST_ONCE_PRESSED));
  EXPECT_EQ(ArcNavigationThrottle::Platform::ARC,
            ArcNavigationThrottle::GetDestinationPlatform(
                non_chrome_app,
                ArcNavigationThrottle::PickerAction::PREFERRED_ACTIVITY_FOUND));
}

}  // namespace arc
