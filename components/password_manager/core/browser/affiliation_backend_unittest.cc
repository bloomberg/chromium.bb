// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_backend.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/password_manager/core/browser/facet_manager.h"
#include "components/password_manager/core/browser/fake_affiliation_api.h"
#include "components/password_manager/core/browser/mock_affiliation_consumer.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

const char kTestFacetURIAlpha1[] = "https://one.alpha.example.com";
const char kTestFacetURIAlpha2[] = "https://two.alpha.example.com";
const char kTestFacetURIAlpha3[] = "https://three.alpha.example.com";
const char kTestFacetURIBeta1[] = "https://one.beta.example.com";
const char kTestFacetURIBeta2[] = "https://two.beta.example.com";
const char kTestFacetURIGamma1[] = "https://gamma.example.com";

AffiliatedFacets GetTestEquivalenceClassAlpha() {
  AffiliatedFacets affiliated_facets;
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha3));
  return affiliated_facets;
}

AffiliatedFacets GetTestEquivalenceClassBeta() {
  AffiliatedFacets affiliated_facets;
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIBeta2));
  return affiliated_facets;
}

AffiliatedFacets GetTestEquivalenceClassGamma() {
  AffiliatedFacets affiliated_facets;
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));
  return affiliated_facets;
}

base::TimeDelta GetCacheHardExpiryPeriod() {
  return base::TimeDelta::FromHours(FacetManager::kCacheHardExpiryInHours);
}

base::TimeDelta GetCacheSoftExpiryPeriod() {
  return base::TimeDelta::FromHours(FacetManager::kCacheSoftExpiryInHours);
}

// Returns a smallest time difference that this test cares about.
base::TimeDelta Epsilon() {
  return base::TimeDelta::FromMicroseconds(1);
}

}  // namespace

class AffiliationBackendTest : public testing::Test {
 public:
  AffiliationBackendTest()
      : consumer_task_runner_(new base::TestSimpleTaskRunner),
        backend_task_runner_(new base::TestMockTimeTaskRunner),
        backend_task_runner_handle_(backend_task_runner_),
        backend_(new AffiliationBackend(NULL,
                                        backend_task_runner_->GetMockClock())) {
  }
  ~AffiliationBackendTest() override {}

 protected:
  void GetAffiliations(MockAffiliationConsumer* consumer,
                       const FacetURI& facet_uri,
                       bool cached_only) {
    backend_->GetAffiliations(facet_uri, cached_only,
                              consumer->GetResultCallback(),
                              consumer_task_runner());
  }

  void Prefetch(const FacetURI& facet_uri, base::Time keep_fresh_until) {
    backend_->Prefetch(facet_uri, keep_fresh_until);
  }

  void CancelPrefetch(const FacetURI& facet_uri, base::Time keep_fresh_until) {
    backend_->CancelPrefetch(facet_uri, keep_fresh_until);
  }

  void ExpectAndCompleteFetch(const FacetURI& expected_requested_facet_uri) {
    ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
    EXPECT_THAT(fake_affiliation_api()->GetNextRequestedFacets(),
                testing::UnorderedElementsAre(expected_requested_facet_uri));
    fake_affiliation_api()->ServeNextRequest();
  }

  void ExpectFailureWithoutFetch(MockAffiliationConsumer* consumer) {
    ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
    consumer->ExpectFailure();
    consumer_task_runner_->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(consumer);
  }

  void GetAffiliationsAndExpectFetchAndThenResult(
      const FacetURI& facet_uri,
      const AffiliatedFacets& expected_result) {
    GetAffiliations(mock_consumer(), facet_uri, false);
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri));
    mock_consumer()->ExpectSuccessWithResult(expected_result);
    consumer_task_runner_->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_consumer());
  }

  void GetAffiliationsAndExpectResultWithoutFetch(
      const FacetURI& facet_uri,
      bool cached_only,
      const AffiliatedFacets& expected_result) {
    GetAffiliations(mock_consumer(), facet_uri, cached_only);
    ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
    mock_consumer()->ExpectSuccessWithResult(expected_result);
    consumer_task_runner_->RunUntilIdle();
    testing::Mock::VerifyAndClearExpectations(mock_consumer());
  }

  void GetAffiliationsAndExpectFailureWithoutFetch(const FacetURI& facet_uri) {
    GetAffiliations(mock_consumer(), facet_uri, true /* cached_only */);
    ASSERT_NO_FATAL_FAILURE(ExpectFailureWithoutFetch(mock_consumer()));
  }

  void PrefetchAndExpectFetch(const FacetURI& facet_uri,
                              base::Time keep_fresh_until) {
    Prefetch(facet_uri, keep_fresh_until);
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri));
  }

  // Verifies that both on-demand and cached-only GetAffiliations() requests for
  // each facet in |affiliated_facets| are served from cache with no fetches.
  void ExpectThatEquivalenceClassIsServedFromCache(
      const AffiliatedFacets& affiliated_facets) {
    for (const FacetURI& facet_uri : affiliated_facets) {
      SCOPED_TRACE(facet_uri);
      ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
          facet_uri, false /* cached_only */, affiliated_facets));
      ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
          facet_uri, true /* cached_only */, affiliated_facets));
    }
  }

  void DestroyBackend() { backend_.reset(); }

  void AdvanceTime(base::TimeDelta delta) {
    backend_task_runner_->FastForwardBy(delta);
  }

  size_t backend_facet_manager_count() {
    return backend()->facet_manager_count_for_testing();
  }

  bool IsCachedDataFreshForFacet(const FacetURI& facet_uri) {
    scoped_ptr<base::Clock> clock(backend_task_runner_->GetMockClock());
    return FacetManager(facet_uri, backend(), clock.get()).IsCachedDataFresh();
  }

  bool IsCachedDataNearStaleForFacet(const FacetURI& facet_uri) {
    scoped_ptr<base::Clock> clock(backend_task_runner_->GetMockClock());
    return FacetManager(facet_uri, backend(), clock.get())
        .IsCachedDataNearStale();
  }

  AffiliationBackend* backend() { return backend_.get(); }

  base::TestMockTimeTaskRunner* backend_task_runner() {
    return backend_task_runner_.get();
  }

  MockAffiliationConsumer* mock_consumer() { return &mock_consumer_; }

  base::TestSimpleTaskRunner* consumer_task_runner() {
    return consumer_task_runner_.get();
  }

  ScopedFakeAffiliationAPI* fake_affiliation_api() {
    return &fake_affiliation_api_;
  }

 private:
  // testing::Test:
  void SetUp() override {
    base::FilePath database_path;
    ASSERT_TRUE(CreateTemporaryFile(&database_path));
    backend_->Initialize(database_path);

    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassAlpha());
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassBeta());
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassGamma());
  }

  ScopedFakeAffiliationAPI fake_affiliation_api_;
  MockAffiliationConsumer mock_consumer_;
  scoped_refptr<base::TestSimpleTaskRunner> consumer_task_runner_;

  scoped_refptr<base::TestMockTimeTaskRunner> backend_task_runner_;
  base::ThreadTaskRunnerHandle backend_task_runner_handle_;

  scoped_ptr<AffiliationBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationBackendTest);
};

TEST_F(AffiliationBackendTest, OnDemandRequestSucceedsWithFetch) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));
  EXPECT_EQ(0u, backend_facet_manager_count());

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1),
      GetTestEquivalenceClassBeta()));
  EXPECT_EQ(0u, backend_facet_manager_count());
}

// This test also verifies that the FacetManager is immediately discarded.
TEST_F(AffiliationBackendTest, CachedOnlyRequestFailsDueToCacheMiss) {
  GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, PrefetchTriggersInitialFetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));
}

// This test also verifies that the FacetManager is immediately discarded.
TEST_F(AffiliationBackendTest, ExpiredPrefetchTriggersNoInitialFetch) {
  // Prefetch intervals are open from the right, thus intervals ending Now() are
  // already expired.
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
           backend_task_runner()->Now());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(fake_affiliation_api()->HasPendingRequest());
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());
}

// One additional GetAffiliations() and one Prefetch() request for unrelated
// facets come in while the network fetch triggered by the first request is in
// flight. There should be no simultaneous requests, and the additional facets
// should be queried together in a second fetch after the first fetch completes.
TEST_F(AffiliationBackendTest, ConcurrentUnrelatedRequests) {
  FacetURI facet_alpha(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_beta(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  FacetURI facet_gamma(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  MockAffiliationConsumer second_consumer;
  GetAffiliations(mock_consumer(), facet_alpha, false);
  GetAffiliations(&second_consumer, facet_beta, false);
  Prefetch(facet_gamma, base::Time::Max());

  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_alpha));
  ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
  EXPECT_THAT(fake_affiliation_api()->GetNextRequestedFacets(),
              testing::UnorderedElementsAre(facet_beta, facet_gamma));
  fake_affiliation_api()->ServeNextRequest();

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  second_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassBeta());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // Now that the two GetAffiliation() requests have been completed, the first
  // two FacetManagers should be discarded. The third FacetManager corresponding
  // to the prefetched facet should be kept.
  EXPECT_GE(1u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CacheServesSubsequentRequestForSameFacet) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), false /* cached_only */,
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), true /* cached_only */,
      GetTestEquivalenceClassAlpha()));

  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CacheServesSubsequentRequestForAffiliatedFacet) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  EXPECT_EQ(0u, backend_facet_manager_count());
}

TEST_F(AffiliationBackendTest, CacheServesRequestsForPrefetchedFacets) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), false /* cached_only */,
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), true /* cached_only */,
      GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest,
       CacheServesRequestsForFacetsAffiliatedWithPrefetchedFacets) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

// A second GetAffiliations() request for the same facet and a third request
// for an affiliated facet comes in while the network fetch triggered by the
// first request is in flight.
//
// There should be no simultaneous requests, and once the fetch completes, all
// three requests should be served without further fetches (they have the data).
TEST_F(AffiliationBackendTest,
       CacheServesConcurrentRequestsForAffiliatedFacets) {
  FacetURI facet_uri1(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  MockAffiliationConsumer second_consumer;
  MockAffiliationConsumer third_consumer;
  GetAffiliations(mock_consumer(), facet_uri1, false);
  GetAffiliations(&second_consumer, facet_uri1, false);
  GetAffiliations(&third_consumer, facet_uri2, false);

  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri1));
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  second_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  third_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  EXPECT_EQ(0u, backend_facet_manager_count());
}

// A second Prefetch() request for the same facet and a third request for an
// affiliated facet comes in while the initial fetch triggered by the first
// request is in flight.
//
// There should be no simultaneous requests, and once the fetch completes, there
// should be no further initial fetches as the data needed is already there.
TEST_F(AffiliationBackendTest,
       CacheServesConcurrentPrefetchesForAffiliatedFacets) {
  FacetURI facet_uri1(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_uri2(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));

  Prefetch(facet_uri1, base::Time::Max());
  Prefetch(facet_uri1, base::Time::Max());
  Prefetch(facet_uri2, base::Time::Max());

  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_uri1));
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest, SimpleCacheExpiryWithoutPrefetches) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());

  EXPECT_TRUE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(Epsilon());

  // After the data becomes stale, the cached-only request should fail, but the
  // subsequent on-demand request should fetch the data again and succeed.
  EXPECT_FALSE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  EXPECT_EQ(0u, backend_facet_manager_count());
}

// A Prefetch() request for a finite period. It should trigger an initial fetch
// and exactly one refetch, as the Prefetch() request expires exactly when the
// cached data obtained with the refetch expires.
TEST_F(AffiliationBackendTest,
       PrefetchTriggersOneInitialFetchAndOneRefetchBeforeExpiring) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      backend_task_runner()->Now() + GetCacheHardExpiryPeriod() +
          GetCacheSoftExpiryPeriod()));

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  EXPECT_FALSE(fake_affiliation_api()->HasPendingRequest());
  EXPECT_FALSE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  AdvanceTime(Epsilon());

  EXPECT_TRUE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(
      ExpectAndCompleteFetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  AdvanceTime(GetCacheHardExpiryPeriod() - Epsilon());

  EXPECT_TRUE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  EXPECT_TRUE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(Epsilon());

  // The data should be allowed to expire and the FacetManager discarded.
  EXPECT_FALSE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(fake_affiliation_api()->HasPendingRequest());
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)));

  // However, a subsequent on-demand request should be able to trigger a fetch.
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));
}

// Affiliation data for prefetched facets should be automatically refetched once
// every 23 hours, and GetAffiliations() requests regarding affiliated facets
// should be continuously served from cache.
TEST_F(AffiliationBackendTest, PrefetchTriggersPeriodicRefetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  for (int cycle = 0; cycle < 3; ++cycle) {
    SCOPED_TRACE(cycle);

    AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

    EXPECT_FALSE(fake_affiliation_api()->HasPendingRequest());
    EXPECT_TRUE(IsCachedDataFreshForFacet(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    EXPECT_FALSE(IsCachedDataNearStaleForFacet(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
        GetTestEquivalenceClassAlpha()));

    AdvanceTime(Epsilon());

    EXPECT_TRUE(IsCachedDataNearStaleForFacet(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(
        FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
    ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
        GetTestEquivalenceClassAlpha()));
  }
}

TEST_F(AffiliationBackendTest,
       PrefetchTriggersNoInitialFetchIfDataIsAlreadyFresh) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  EXPECT_FALSE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max());
  EXPECT_FALSE(fake_affiliation_api()->HasPendingRequest());
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest, CancelPrefetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  // Cancel the prefetch the last microsecond before a refetch would take place.
  backend()->CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                            base::Time::Max());
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(fake_affiliation_api()->HasPendingRequest());
  EXPECT_TRUE(backend_task_runner()->HasPendingTask());

  AdvanceTime(GetCacheHardExpiryPeriod() - GetCacheSoftExpiryPeriod() +
              Epsilon());

  // The data should be allowed to expire.
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());
  EXPECT_TRUE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFailureWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2)));
}

TEST_F(AffiliationBackendTest, CancelDuplicatePrefetch) {
  ASSERT_NO_FATAL_FAILURE(PrefetchAndExpectFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max()));
  Prefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1), base::Time::Max());

  AdvanceTime(GetCacheSoftExpiryPeriod() - Epsilon());

  // Cancel the prefetch the last microsecond before a refetch would take place.
  backend()->CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                            base::Time::Max());

  AdvanceTime(Epsilon());

  // However, there is a second Prefetch() request which should keep the data
  // fresh.
  EXPECT_EQ(1u, backend_facet_manager_count());
  EXPECT_TRUE(IsCachedDataNearStaleForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(
      ExpectAndCompleteFetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));

  AdvanceTime(GetCacheHardExpiryPeriod() - GetCacheSoftExpiryPeriod());

  EXPECT_TRUE(IsCachedDataFreshForFacet(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1)));
  ASSERT_NO_FATAL_FAILURE(ExpectThatEquivalenceClassIsServedFromCache(
      GetTestEquivalenceClassAlpha()));
}

// Canceling a non-existing prefetch request for a non-prefetched facet.
TEST_F(AffiliationBackendTest, CancelingNonExistingPrefetchIsSilentlyIgnored) {
  CancelPrefetch(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                 backend_task_runner()->Now() + base::TimeDelta::FromHours(24));
  EXPECT_EQ(0u, backend_facet_manager_count());
  EXPECT_FALSE(fake_affiliation_api()->HasPendingRequest());
  EXPECT_FALSE(backend_task_runner()->HasPendingTask());
}

TEST_F(AffiliationBackendTest, NothingExplodesWhenShutDownDuringFetch) {
  GetAffiliations(mock_consumer(),
                  FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
                  false /* cached_only */);
  ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
  fake_affiliation_api()->IgnoreNextRequest();
  DestroyBackend();
}

TEST_F(AffiliationBackendTest,
       FailureCallbacksAreCalledIfBackendIsDestroyedWithPendingRequest) {
  GetAffiliations(mock_consumer(),
                  FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
                  false /* cached_only */);
  ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
  // Currently, a GetAffiliations() request can only be blocked due to fetch in
  // flight -- so emulate this condition when destroying the backend.
  fake_affiliation_api()->IgnoreNextRequest();
  DestroyBackend();
  mock_consumer()->ExpectFailure();
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

}  // namespace password_manager
