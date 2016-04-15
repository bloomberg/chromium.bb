// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>

#include "chrome/browser/push_messaging/background_budget_service.h"
#include "chrome/browser/push_messaging/background_budget_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestOrigin[] = "https://example.com";
const char kTestData[] = "1111101111";

}  // namespace

class BackgroundBudgetServiceTest : public testing::Test {
 public:
  BackgroundBudgetServiceTest() {}
  ~BackgroundBudgetServiceTest() override {}

  BackgroundBudgetService* service() {
    return BackgroundBudgetServiceFactory::GetForProfile(&profile_);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

TEST_F(BackgroundBudgetServiceTest, GetBudgetFailure) {
  const GURL origin(kTestOrigin);

  std::string budget_string = service()->GetBudget(origin);

  EXPECT_EQ(std::string(), budget_string);
}

TEST_F(BackgroundBudgetServiceTest, GetBudgetSuccess) {
  const GURL origin(kTestOrigin);

  service()->StoreBudget(origin, kTestData);

  std::string budget_string = service()->GetBudget(origin);

  EXPECT_EQ(kTestData, budget_string);
}
