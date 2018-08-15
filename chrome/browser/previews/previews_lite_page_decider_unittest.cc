// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_decider.h"

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

class PreviewsLitePageDeciderTest : public testing::Test {
 protected:
  PreviewsLitePageDeciderTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
};

TEST_F(PreviewsLitePageDeciderTest, TestServerUnavailable) {
  const base::TimeTicks kNow = base::TimeTicks::Now();
  struct TestCase {
    base::TimeTicks retry_at;
    base::TimeTicks now;
    bool want_is_unavailable;
  };
  const TestCase kTestCases[]{
      {
          kNow - base::TimeDelta::FromMinutes(1), kNow, false,
      },
      {
          kNow + base::TimeDelta::FromMinutes(1), kNow, true,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    std::unique_ptr<PreviewsLitePageDecider> decider =
        std::make_unique<PreviewsLitePageDecider>();
    decider->SetServerUnavailableUntil(test_case.retry_at);
    EXPECT_EQ(decider->IsServerUnavailable(test_case.now),
              test_case.want_is_unavailable);
  }
}

TEST_F(PreviewsLitePageDeciderTest, TestSingleBypass) {
  const std::string kUrl = "http://test.com";
  struct TestCase {
    std::string add_url;
    base::TimeDelta clock_advance;
    std::string check_url;
    bool want_check;
  };
  const TestCase kTestCases[]{
      {
          kUrl, base::TimeDelta::FromMinutes(1), kUrl, true,
      },
      {
          kUrl, base::TimeDelta::FromMinutes(6), kUrl, false,
      },
      {
          "bad", base::TimeDelta::FromMinutes(1), kUrl, false,
      },
      {
          "bad", base::TimeDelta::FromMinutes(6), kUrl, false,
      },
      {
          kUrl, base::TimeDelta::FromMinutes(1), "bad", false,
      },
      {
          kUrl, base::TimeDelta::FromMinutes(6), "bad", false,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    std::unique_ptr<PreviewsLitePageDecider> decider =
        std::make_unique<PreviewsLitePageDecider>();
    std::unique_ptr<base::SimpleTestTickClock> clock =
        std::make_unique<base::SimpleTestTickClock>();
    decider->SetClockForTesting(clock.get());

    decider->AddSingleBypass(test_case.add_url);
    clock->Advance(test_case.clock_advance);
    EXPECT_EQ(decider->CheckSingleBypass(test_case.check_url),
              test_case.want_check);
  }
}
