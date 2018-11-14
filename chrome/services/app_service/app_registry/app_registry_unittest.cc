// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/services/app_service/app_registry/app_registry.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestAppId1[] = "test.app.id.1";
const char kTestAppId2[] = "test.app.id.2";
}  // namespace

namespace apps {

class AppRegistryTest : public testing::Test {
 protected:
  std::unique_ptr<AppRegistry> CreateAppRegistry() {
    auto pref_service = std::make_unique<TestingPrefServiceSimple>();
    AppRegistry::RegisterPrefs(pref_service->registry());
    return std::make_unique<AppRegistry>(std::move(pref_service));
  }
};

TEST_F(AppRegistryTest, SetAppPreferred) {
  auto registry = CreateAppRegistry();

  EXPECT_EQ(apps::mojom::AppPreferred::kUnknown,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kUnknown,
            registry->GetIfAppPreferredForTesting(kTestAppId2));

  registry->SetAppPreferred(kTestAppId1, apps::mojom::AppPreferred::kPreferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kUnknown,
            registry->GetIfAppPreferredForTesting(kTestAppId2));

  registry->SetAppPreferred(kTestAppId1,
                            apps::mojom::AppPreferred::kNotPreferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kUnknown,
            registry->GetIfAppPreferredForTesting(kTestAppId2));

  registry->SetAppPreferred(kTestAppId2, apps::mojom::AppPreferred::kPreferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId2));

  registry->SetAppPreferred(kTestAppId2,
                            apps::mojom::AppPreferred::kNotPreferred);
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId1));
  EXPECT_EQ(apps::mojom::AppPreferred::kNotPreferred,
            registry->GetIfAppPreferredForTesting(kTestAppId2));
}

}  // namespace apps
