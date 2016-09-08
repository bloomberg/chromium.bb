// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/budget_service/budget_manager.h"
#include "chrome/browser/budget_service/budget_manager_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/budget_service/budget_service.mojom.h"

namespace {

const char kTestOrigin[] = "https://example.com";
const double kTestSES = 48.0;

}  // namespace

class BudgetManagerTest : public testing::Test {
 public:
  ~BudgetManagerTest() override {}

  BudgetManager* GetManager() {
    return BudgetManagerFactory::GetForProfile(&profile_);
  }

  void SetSiteEngagementScore(const GURL& url, double score) {
    SiteEngagementService* service = SiteEngagementService::Get(&profile_);
    service->ResetScoreForURL(url, score);
  }

  Profile* profile() { return &profile_; }

  void StatusCallback(base::Closure run_loop_closure, bool success) {
    success_ = success;
    run_loop_closure.Run();
  }

  bool ReserveBudget(blink::mojom::BudgetOperationType type) {
    const GURL origin(kTestOrigin);
    base::RunLoop run_loop;
    GetManager()->Reserve(
        origin, type,
        base::Bind(&BudgetManagerTest::StatusCallback, base::Unretained(this),
                   run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  bool ConsumeBudget(blink::mojom::BudgetOperationType type) {
    const GURL origin(kTestOrigin);
    base::RunLoop run_loop;
    GetManager()->Consume(
        origin, type,
        base::Bind(&BudgetManagerTest::StatusCallback, base::Unretained(this),
                   run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  // Members for callbacks to set.
  bool success_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

TEST_F(BudgetManagerTest, GetBudgetConsumedOverTime) {
  // Set initial SES. The first time we try to spend budget, the
  // engagement award will be granted which is 48.0.
  const GURL origin(kTestOrigin);
  SetSiteEngagementScore(origin, kTestSES);
  const blink::mojom::BudgetOperationType type =
      blink::mojom::BudgetOperationType::SILENT_PUSH;

  // Spend for 24 silent push messages. This should consume all the original
  // budget grant.
  for (int i = 0; i < 24; i++)
    ASSERT_TRUE(ReserveBudget(type));

  // Try to send one final silent push. The origin should be out of budget.
  ASSERT_FALSE(ReserveBudget(type));

  // Try to consume for the 24 messages reserved.
  for (int i = 0; i < 24; i++)
    ASSERT_TRUE(ConsumeBudget(type));

  // The next consume should fail, since there is no reservation or budget
  // available.
  ASSERT_FALSE(ConsumeBudget(type));
}
