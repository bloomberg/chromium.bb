// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This test focuses on functionality implemented in AffiliationService
// itself. More thorough The AffiliationBackend is tested in-depth separarately.

#include "components/password_manager/core/browser/affiliation_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/password_manager/core/browser/fake_affiliation_api.h"
#include "components/password_manager/core/browser/mock_affiliation_consumer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using StrategyOnCacheMiss = AffiliationService::StrategyOnCacheMiss;

const char kTestFacetURIAlpha1[] = "https://one.alpha.example.com";
const char kTestFacetURIAlpha2[] = "https://two.alpha.example.com";
const char kTestFacetURIAlpha3[] = "https://three.alpha.example.com";
const char kTestFacetURIBeta1[] = "https://one.beta.example.com";

AffiliatedFacets GetTestEquivalenceClassAlpha() {
  AffiliatedFacets affiliated_facets;
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1));
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha2));
  affiliated_facets.push_back(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha3));
  return affiliated_facets;
}

}  // namespace

class AffiliationServiceTest : public testing::Test {
 public:
  AffiliationServiceTest()
      : main_task_runner_(new base::TestSimpleTaskRunner),
        main_task_runner_handle_(main_task_runner_),
        background_task_runner_(new base::TestMockTimeTaskRunner) {}
  ~AffiliationServiceTest() override {}

 protected:
  void DestroyService() { service_.reset(); }

  AffiliationService* service() { return service_.get(); }
  MockAffiliationConsumer* mock_consumer() { return &mock_consumer_; }

  base::TestSimpleTaskRunner* main_task_runner() {
    return main_task_runner_.get();
  }

  base::TestMockTimeTaskRunner* background_task_runner() {
    return background_task_runner_.get();
  }

  ScopedFakeAffiliationAPI* fake_affiliation_api() {
    return &fake_affiliation_api_;
  }

 private:
  // testing::Test:
  void SetUp() override {
    base::FilePath database_path;
    ASSERT_TRUE(CreateTemporaryFile(&database_path));
    service_.reset(new AffiliationService(background_task_runner()));
    service_->Initialize(nullptr, database_path);
    // Note: the background task runner is purposely not pumped here, so that
    // the tests also verify that the service can be used synchronously right
    // away after having been constructed.
    fake_affiliation_api_.AddTestEquivalenceClass(
        GetTestEquivalenceClassAlpha());
  }

  void TearDown() override {
    // The service uses DeleteSoon to asynchronously destroy its backend. Pump
    // the background thread to make sure destruction actually takes place.
    DestroyService();
    background_task_runner_->RunUntilIdle();
  }

  scoped_refptr<base::TestSimpleTaskRunner> main_task_runner_;
  base::ThreadTaskRunnerHandle main_task_runner_handle_;
  scoped_refptr<base::TestMockTimeTaskRunner> background_task_runner_;

  ScopedFakeAffiliationAPI fake_affiliation_api_;
  MockAffiliationConsumer mock_consumer_;

  std::unique_ptr<AffiliationService> service_;

  DISALLOW_COPY_AND_ASSIGN(AffiliationServiceTest);
};

TEST_F(AffiliationServiceTest, GetAffiliations) {
  // The first request allows on-demand fetching, and should trigger a fetch.
  // Then, it should succeed after the fetch is complete.
  service()->GetAffiliations(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK,
                             mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_TRUE(fake_affiliation_api()->HasPendingRequest());
  fake_affiliation_api()->ServeNextRequest();

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  main_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // The second request should be (and can be) served from cache.
  service()->GetAffiliations(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                             StrategyOnCacheMiss::FAIL,
                             mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  mock_consumer()->ExpectSuccessWithResult(GetTestEquivalenceClassAlpha());
  main_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());

  // The third request is also restricted to the cache, but cannot be served
  // from cache, thus it should fail.
  service()->GetAffiliations(FacetURI::FromCanonicalSpec(kTestFacetURIBeta1),
                             StrategyOnCacheMiss::FAIL,
                             mock_consumer()->GetResultCallback());

  background_task_runner()->RunUntilIdle();
  ASSERT_FALSE(fake_affiliation_api()->HasPendingRequest());

  mock_consumer()->ExpectFailure();
  main_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

TEST_F(AffiliationServiceTest, ShutdownWhileTasksArePosted) {
  service()->GetAffiliations(FacetURI::FromCanonicalSpec(kTestFacetURIAlpha1),
                             StrategyOnCacheMiss::FETCH_OVER_NETWORK,
                             mock_consumer()->GetResultCallback());
  EXPECT_TRUE(background_task_runner()->HasPendingTask());

  DestroyService();
  background_task_runner()->RunUntilIdle();

  mock_consumer()->ExpectFailure();
  main_task_runner()->RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(mock_consumer());
}

}  // namespace password_manager
