// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

TEST(ArcNavigationThrottleTest, TestShouldOverrideUrlLoading) {
  // A navigation within the same domain shouldn't be overridden.
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://google.com"), GURL("http://google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://google.com"), GURL("http://a.google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://b.google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com"), GURL("http://b.c.google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.b.google.com"), GURL("http://c.google.com/")));

  // If either of two paramters is empty, the function should return false.
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL(), GURL("http://a.google.com/")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://a.google.com/"), GURL()));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL(), GURL()));

  // A navigation not within the same domain can be overridden.
  EXPECT_TRUE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.google.com"), GURL("http://www.not-google.com/")));
  EXPECT_TRUE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.not-google.com"), GURL("http://www.google.com/")));

  // A navigation with neither an http nor https scheme cannot be overriden.
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome-extension://fake_document"), GURL("http://www.a.com")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("http://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome-extension://fake_document"), GURL("https://www.a.com")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("https://www.a.com"), GURL("chrome-extension://fake_document")));
  EXPECT_FALSE(ArcNavigationThrottle::ShouldOverrideUrlLoadingForTesting(
      GURL("chrome-extension://fake_a"), GURL("chrome-extension://fake_b")));
}

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

TEST(ArcNavigationThrottleTest, TestGetDestinationPlatform) {
  const std::string chrome_app =
      ArcIntentHelperBridge::kArcIntentHelperPackageName;
  const std::string non_chrome_app = "fake_package";

  // When the CloseReason is either ERROR or DIALOG_DEACTIVATED we MUST stay in
  // Chrome not taking into account the selected_app_package.
  EXPECT_EQ(ArcNavigationThrottle::Platform::CHROME,
            ArcNavigationThrottle::GetDestinationPlatform(
                chrome_app, ArcNavigationThrottle::CloseReason::ERROR));
  EXPECT_EQ(ArcNavigationThrottle::Platform::CHROME,
            ArcNavigationThrottle::GetDestinationPlatform(
                non_chrome_app, ArcNavigationThrottle::CloseReason::ERROR));
  EXPECT_EQ(
      ArcNavigationThrottle::Platform::CHROME,
      ArcNavigationThrottle::GetDestinationPlatform(
          chrome_app, ArcNavigationThrottle::CloseReason::DIALOG_DEACTIVATED));
  EXPECT_EQ(ArcNavigationThrottle::Platform::CHROME,
            ArcNavigationThrottle::GetDestinationPlatform(
                non_chrome_app,
                ArcNavigationThrottle::CloseReason::DIALOG_DEACTIVATED));

  // Under any other CloseReason, stay in Chrome only if the package is Chrome.
  // Otherwise redirect to ARC.
  EXPECT_EQ(
      ArcNavigationThrottle::Platform::CHROME,
      ArcNavigationThrottle::GetDestinationPlatform(
          chrome_app, ArcNavigationThrottle::CloseReason::ALWAYS_PRESSED));
  EXPECT_EQ(
      ArcNavigationThrottle::Platform::CHROME,
      ArcNavigationThrottle::GetDestinationPlatform(
          chrome_app, ArcNavigationThrottle::CloseReason::JUST_ONCE_PRESSED));
  EXPECT_EQ(ArcNavigationThrottle::Platform::CHROME,
            ArcNavigationThrottle::GetDestinationPlatform(
                chrome_app,
                ArcNavigationThrottle::CloseReason::PREFERRED_ACTIVITY_FOUND));

  // Go to ARC on any other case.
  EXPECT_EQ(
      ArcNavigationThrottle::Platform::ARC,
      ArcNavigationThrottle::GetDestinationPlatform(
          non_chrome_app, ArcNavigationThrottle::CloseReason::ALWAYS_PRESSED));
  EXPECT_EQ(ArcNavigationThrottle::Platform::ARC,
            ArcNavigationThrottle::GetDestinationPlatform(
                non_chrome_app,
                ArcNavigationThrottle::CloseReason::JUST_ONCE_PRESSED));
  EXPECT_EQ(ArcNavigationThrottle::Platform::ARC,
            ArcNavigationThrottle::GetDestinationPlatform(
                non_chrome_app,
                ArcNavigationThrottle::CloseReason::PREFERRED_ACTIVITY_FOUND));
}

TEST(ArcNavigationThrottleTest, TestIsSwapElementsNeeded) {
  std::pair<size_t, size_t> indices;
  for (size_t i = 1; i <= ArcNavigationThrottle::kMaxAppResults; ++i) {
    // When Chrome is the first element, swap is unnecessary.
    std::vector<mojom::IntentHandlerInfoPtr> handlers = CreateArray(i, 0);
    EXPECT_FALSE(
        ArcNavigationThrottle::IsSwapElementsNeeded(handlers, &indices))
        << i;

    // When Chrome is within the first |kMaxAppResults| elements, swap is
    // unnecessary.
    handlers = CreateArray(i, i - 1);
    EXPECT_FALSE(
        ArcNavigationThrottle::IsSwapElementsNeeded(handlers, &indices))
        << i;
  }

  for (size_t i = ArcNavigationThrottle::kMaxAppResults + 1;
       i < ArcNavigationThrottle::kMaxAppResults * 2; ++i) {
    // When Chrome is within the first |kMaxAppResults| elements, swap is
    // unnecessary.
    std::vector<mojom::IntentHandlerInfoPtr> handlers = CreateArray(i, 0);
    EXPECT_FALSE(
        ArcNavigationThrottle::IsSwapElementsNeeded(handlers, &indices))
        << i;

    // When Chrome is the |kMaxAppResults|-th element, swap is unnecessary.
    handlers = CreateArray(i, ArcNavigationThrottle::kMaxAppResults - 1);
    EXPECT_FALSE(
        ArcNavigationThrottle::IsSwapElementsNeeded(handlers, &indices))
        << i;

    // When Chrome is not within the first |kMaxAppResults| elements, swap is
    // necessary.
    handlers = CreateArray(i, i - 1);
    indices.first = indices.second = 0;
    EXPECT_TRUE(ArcNavigationThrottle::IsSwapElementsNeeded(handlers, &indices))
        << i;
    EXPECT_EQ(ArcNavigationThrottle::kMaxAppResults - 1u, indices.first) << i;
    EXPECT_EQ(i - 1, indices.second) << i;
  }

  for (size_t i = 0; i <= ArcNavigationThrottle::kMaxAppResults * 2; ++i) {
    // When Chrome does not exist in |handlers|, swap is unnecessary.
    std::vector<mojom::IntentHandlerInfoPtr> handlers = CreateArray(i, i);
    EXPECT_FALSE(
        ArcNavigationThrottle::IsSwapElementsNeeded(handlers, &indices))
        << i;
  }
}

}  // namespace arc
