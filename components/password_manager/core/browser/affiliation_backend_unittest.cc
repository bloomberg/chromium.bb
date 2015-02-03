// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_backend.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_simple_task_runner.h"
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

}  // namespace

class AffiliationBackendTest : public testing::Test {
 public:
  AffiliationBackendTest()
      : consumer_task_runner_(new base::TestSimpleTaskRunner),
        clock_(new base::SimpleTestClock) {}
  ~AffiliationBackendTest() override {}

 protected:
  void GetAffiliations(MockAffiliationConsumer* consumer,
                       const FacetURI& facet_uri,
                       bool cached_only) {
    backend_->GetAffiliations(facet_uri, cached_only,
                              consumer->GetResultCallback(),
                              consumer_task_runner());
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

  void DestroyBackend() {
    clock_ = nullptr;
    backend_.reset();
  }

  void AdvanceTime(base::TimeDelta delta) { clock_->Advance(delta); }

  AffiliationBackend* backend() { return backend_.get(); }
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
    clock_->Advance(base::TimeDelta::FromMicroseconds(1));
    backend_.reset(new AffiliationBackend(NULL, make_scoped_ptr(clock_)));

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

  base::SimpleTestClock* clock_;  // Owned by |backend_|.
  scoped_ptr<AffiliationBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationBackendTest);
};

TEST_F(AffiliationBackendTest, OnDemandRequestSucceedsWithFetch) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIBeta1),
      GetTestEquivalenceClassBeta()));
}

TEST_F(AffiliationBackendTest, CachedOnlyRequestFailsOnCacheMiss) {
  GetAffiliations(mock_consumer(),
                  FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
                  true /* cached_only */);
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());
  mock_consumer()->ExpectFailure();
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

// Two additional requests for unrelated facets come in while the network fetch
// triggered by the first request is in flight. There should be no simultaneous
// requests, and the additional facets should be queried together in a second
// fetch after the first fetch completes.
TEST_F(AffiliationBackendTest, ConcurrentUnrelatedRequests) {
  FacetURI facet_alpha(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  FacetURI facet_beta(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1));
  FacetURI facet_gamma(FacetURI::FromCanonicalSpec(kTestFacetURIGamma1));

  MockAffiliationConsumer second_consumer;
  MockAffiliationConsumer third_consumer;
  GetAffiliations(mock_consumer(), facet_alpha, false);
  GetAffiliations(&second_consumer, facet_beta, false);
  GetAffiliations(&third_consumer, facet_gamma, false);

  ASSERT_NO_FATAL_FAILURE(ExpectAndCompleteFetch(facet_alpha));
  ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
  EXPECT_THAT(fake_affiliation_api()->GetNextRequestedFacets(),
              testing::UnorderedElementsAre(facet_beta, facet_gamma));
  fake_affiliation_api()->ServeNextRequest();

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  second_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassBeta());
  third_consumer.ExpectSuccessWithResult(GetTestEquivalenceClassGamma());
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
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
}

TEST_F(AffiliationBackendTest, CacheServesSubsequentRequestForAffiliatedFacet) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2), false /* cached_only */,
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2), true /* cached_only */,
      GetTestEquivalenceClassAlpha()));
}

TEST_F(AffiliationBackendTest, CacheExpiry) {
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(base::TimeDelta::FromHours(24) -
              base::TimeDelta::FromMicroseconds(1));

  // Before the data becomes stale, both on-demand and cached-only requests are
  // expected to be served from the cache.
  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2), false /* cached_only */,
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2), true /* cached_only */,
      GetTestEquivalenceClassAlpha()));

  AdvanceTime(base::TimeDelta::FromMicroseconds(1));

  // After the data becomes stale, the cached-only request should fail, but the
  // subsequent on-demand request should fetch the data again and succeed.
  GetAffiliations(mock_consumer(),
                  FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
                  true /* cached_only */);
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  mock_consumer()->ExpectFailure();
  consumer_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectFetchAndThenResult(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2),
      GetTestEquivalenceClassAlpha()));

  ASSERT_NO_FATAL_FAILURE(GetAffiliationsAndExpectResultWithoutFetch(
      FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2), true /* cached_only */,
      GetTestEquivalenceClassAlpha()));
}

// A second GetAffiliations() request for the same facet and a third request
// for an affiliated facet comes in while the network fetch triggered by the
// first request is in flight.
//
// There should be no simultaneous requests, so once the fetch completes, all
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
