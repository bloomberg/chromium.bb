// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/site_engagement_eviction_policy.h"

#include <stdint.h>

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/mock_storage_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int64_t kGlobalQuota = 25 * 1024;

}  // namespace

class TestSiteEngagementScoreProvider : public SiteEngagementScoreProvider {
 public:
  TestSiteEngagementScoreProvider() {}

  virtual ~TestSiteEngagementScoreProvider() {}

  double GetScore(const GURL& url) const override {
    const auto& it = engagement_score_map_.find(url);
    if (it != engagement_score_map_.end())
      return it->second;
    return 0.0;
  }

  double GetTotalEngagementPoints() const override {
    double total = 0;
    for (const auto& site : engagement_score_map_)
      total += site.second;
    return total;
  }

  void SetScore(const GURL& origin, double score) {
    engagement_score_map_[origin] = score;
  }

 private:
  std::map<GURL, double> engagement_score_map_;

  DISALLOW_COPY_AND_ASSIGN(TestSiteEngagementScoreProvider);
};

class SiteEngagementEvictionPolicyTest : public testing::Test {
 public:
  SiteEngagementEvictionPolicyTest()
      : score_provider_(new TestSiteEngagementScoreProvider()),
        storage_policy_(new content::MockSpecialStoragePolicy()) {}

  ~SiteEngagementEvictionPolicyTest() override {}

  GURL CalculateEvictionOriginWithExceptions(
      const std::map<GURL, int64_t>& usage,
      const std::set<GURL>& exceptions) {
    return SiteEngagementEvictionPolicy::CalculateEvictionOriginForTests(
        storage_policy_, score_provider_.get(), exceptions, usage,
        kGlobalQuota);
  }

  GURL CalculateEvictionOrigin(const std::map<GURL, int64_t>& usage) {
    return CalculateEvictionOriginWithExceptions(usage, std::set<GURL>());
  }

  TestSiteEngagementScoreProvider* score_provider() {
    return score_provider_.get();
  }

  content::MockSpecialStoragePolicy* storage_policy() {
    return storage_policy_.get();
  }

 private:
  std::unique_ptr<TestSiteEngagementScoreProvider> score_provider_;
  scoped_refptr<content::MockSpecialStoragePolicy> storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(SiteEngagementEvictionPolicyTest);
};

TEST_F(SiteEngagementEvictionPolicyTest, GetEvictionOrigin) {
  GURL url1("http://www.google.com");
  GURL url2("http://www.example.com");
  GURL url3("http://www.spam.me");

  std::map<GURL, int64_t> usage;
  usage[url1] = 10 * 1024;
  usage[url2] = 10 * 1024;
  usage[url3] = 10 * 1024;

  score_provider()->SetScore(url1, 50);
  score_provider()->SetScore(url2, 25);

  // When 3 sites have equal usage, evict the site with the least engagement.
  EXPECT_EQ(url3, CalculateEvictionOrigin(usage));

  usage[url2] = usage[url3] + 10;

  // Now |url2| has the most usage but |url3| has the least engagement score so
  // one of them should be evicted. In this case the heuristic chooses |url3|.
  EXPECT_EQ(url3, CalculateEvictionOrigin(usage));

  // But exceeding allocated usage too much will still result in being evicted
  // even though the engagement with |url2| is higher.
  usage[url2] = 15 * 1024;
  EXPECT_EQ(url2, CalculateEvictionOrigin(usage));

  // When all origins have the same engagement, the origin with the highest
  // usage is evicted.
  score_provider()->SetScore(url1, 50);
  score_provider()->SetScore(url2, 50);
  score_provider()->SetScore(url3, 50);

  usage[url2] = 10 * 1024;
  usage[url3] = 20 * 1024;
  EXPECT_EQ(url3, CalculateEvictionOrigin(usage));
}

// Test that durable and unlimited storage origins are exempt from eviction.
TEST_F(SiteEngagementEvictionPolicyTest, SpecialStoragePolicy) {
  GURL url1("http://www.google.com");
  GURL url2("http://www.example.com");

  std::map<GURL, int64_t> usage;
  usage[url1] = 10 * 1024;
  usage[url2] = 10 * 1024;

  score_provider()->SetScore(url1, 50);
  score_provider()->SetScore(url2, 25);

  EXPECT_EQ(url2, CalculateEvictionOrigin(usage));

  // Durable storage doesn't get evicted.
  storage_policy()->AddDurable(url2);
  EXPECT_EQ(url1, CalculateEvictionOrigin(usage));

  // Unlimited storage doesn't get evicted.
  storage_policy()->AddUnlimited(url1);
  EXPECT_EQ(GURL(), CalculateEvictionOrigin(usage));
}

TEST_F(SiteEngagementEvictionPolicyTest, Exceptions) {
  GURL url1("http://www.google.com");
  GURL url2("http://www.example.com");

  std::map<GURL, int64_t> usage;
  usage[url1] = 10 * 1024;
  usage[url2] = 10 * 1024;

  score_provider()->SetScore(url1, 50);
  score_provider()->SetScore(url2, 25);

  EXPECT_EQ(url2, CalculateEvictionOrigin(usage));

  // The policy should respect exceptions.
  std::set<GURL> exceptions;
  exceptions.insert(url2);
  EXPECT_EQ(url1, CalculateEvictionOriginWithExceptions(usage, exceptions));
}
