// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
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
#include "url/origin.h"

namespace {

const char kTestOrigin[] = "https://example.com";
const double kTestSES = 48.0;

}  // namespace

class BudgetManagerTest : public testing::Test {
 public:
  BudgetManagerTest() : origin_(url::Origin(GURL(kTestOrigin))) {}
  ~BudgetManagerTest() override {}

  BudgetManager* GetManager() {
    return BudgetManagerFactory::GetForProfile(&profile_);
  }

  void SetSiteEngagementScore(double score) {
    SiteEngagementService* service = SiteEngagementService::Get(&profile_);
    service->ResetScoreForURL(GURL(origin().Serialize()), score);
  }

  Profile* profile() { return &profile_; }
  const url::Origin origin() const { return origin_; }
  void SetOrigin(const url::Origin& origin) { origin_ = origin; }

  void GetBudgetCallback(base::Closure run_loop_closure,
                         blink::mojom::BudgetServiceErrorType error,
                         std::vector<blink::mojom::BudgetStatePtr> budget) {
    // The BudgetDatabaseTest class tests all of the budget values returned.
    // This is just checking that the UMA records correctly, so no need to keep
    // track of the budget value.
    success_ = (error == blink::mojom::BudgetServiceErrorType::NONE);
    error_ = error;
    run_loop_closure.Run();
  }

  void ReserveCallback(base::Closure run_loop_closure,
                       blink::mojom::BudgetServiceErrorType error,
                       bool success) {
    success_ = (error == blink::mojom::BudgetServiceErrorType::NONE) && success;
    error_ = error;
    run_loop_closure.Run();
  }

  void ConsumeCallback(base::Closure run_loop_closure, bool success) {
    success_ = success;
    run_loop_closure.Run();
  }

  bool GetBudget() {
    base::RunLoop run_loop;
    GetManager()->GetBudget(
        origin(), base::Bind(&BudgetManagerTest::GetBudgetCallback,
                             base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  bool ReserveBudget(blink::mojom::BudgetOperationType type) {
    base::RunLoop run_loop;
    GetManager()->Reserve(
        origin(), type,
        base::Bind(&BudgetManagerTest::ReserveCallback, base::Unretained(this),
                   run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  bool ConsumeBudget(blink::mojom::BudgetOperationType type) {
    base::RunLoop run_loop;
    GetManager()->Consume(
        origin(), type,
        base::Bind(&BudgetManagerTest::ConsumeCallback, base::Unretained(this),
                   run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  // Members for callbacks to set.
  bool success_;
  blink::mojom::BudgetServiceErrorType error_;

 protected:
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  base::HistogramTester histogram_tester_;
  url::Origin origin_;

  DISALLOW_COPY_AND_ASSIGN(BudgetManagerTest);
};

TEST_F(BudgetManagerTest, GetBudgetConsumedOverTime) {
  // Set initial SES. The first time we try to spend budget, the engagement
  // award will be granted which is 23.04. (kTestSES * maxDaily / maxSES)
  SetSiteEngagementScore(kTestSES);
  const blink::mojom::BudgetOperationType type =
      blink::mojom::BudgetOperationType::SILENT_PUSH;

  // Spend for 11 silent push messages. This should consume all the original
  // budget grant.
  for (int i = 0; i < 11; i++) {
    ASSERT_TRUE(GetBudget());
    ASSERT_TRUE(ReserveBudget(type));
  }

  // Try to send one final silent push. The origin should be out of budget.
  ASSERT_TRUE(GetBudget());
  ASSERT_FALSE(ReserveBudget(type));

  // Try to consume for the 11 messages reserved.
  for (int i = 0; i < 11; i++)
    ASSERT_TRUE(ConsumeBudget(type));

  // The next consume should fail, since there is no reservation or budget
  // available.
  ASSERT_FALSE(ConsumeBudget(type));

  // Check the the UMA recorded for the Reserve calls matches the operations
  // that were executed.
  std::vector<base::Bucket> buckets =
      histogram_tester()->GetAllSamples("Blink.BudgetAPI.Reserve");
  ASSERT_EQ(2U, buckets.size());
  // 1 failed reserve call.
  EXPECT_EQ(0, buckets[0].min);
  EXPECT_EQ(1, buckets[0].count);
  // 11 successful reserve calls.
  EXPECT_EQ(1, buckets[1].min);
  EXPECT_EQ(11, buckets[1].count);

  // Check that the UMA recorded for the GetBudget calls matches the operations
  // that were executed.
  buckets = histogram_tester()->GetAllSamples("Blink.BudgetAPI.QueryBudget");

  int num_samples = 0;
  for (const base::Bucket& bucket : buckets)
    num_samples += bucket.count;
  EXPECT_EQ(12, num_samples);
}

TEST_F(BudgetManagerTest, TestInsecureOrigin) {
  const blink::mojom::BudgetOperationType type =
      blink::mojom::BudgetOperationType::SILENT_PUSH;
  SetOrigin(url::Origin(GURL("http://example.com")));
  SetSiteEngagementScore(kTestSES);

  // Methods on the BudgetManager should only be allowed for secure origins.
  ASSERT_FALSE(ReserveBudget(type));
  ASSERT_EQ(blink::mojom::BudgetServiceErrorType::NOT_SUPPORTED, error_);
  ASSERT_FALSE(ConsumeBudget(type));
}

TEST_F(BudgetManagerTest, TestUniqueOrigin) {
  const blink::mojom::BudgetOperationType type =
      blink::mojom::BudgetOperationType::SILENT_PUSH;
  SetOrigin(url::Origin(GURL("file://example.com:443/etc/passwd")));

  // Methods on the BudgetManager should not be allowed for unique origins.
  ASSERT_FALSE(ReserveBudget(type));
  ASSERT_EQ(blink::mojom::BudgetServiceErrorType::NOT_SUPPORTED, error_);
  ASSERT_FALSE(ConsumeBudget(type));
}
