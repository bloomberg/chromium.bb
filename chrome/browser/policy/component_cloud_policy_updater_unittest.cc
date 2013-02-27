// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/component_cloud_policy_updater.h"

#include <string>

#include "base/basictypes.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "base/test/test_pending_task.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/component_cloud_policy_store.h"
#include "chrome/browser/policy/policy_builder.h"
#include "chrome/browser/policy/proto/chrome_extension_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/resource_cache.h"
#include "net/base/net_errors.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::Mock;

namespace policy {

namespace {

const char kTestExtension[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtension2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kTestDownload[] = "http://example.com/getpolicy?id=123";
const char kTestDownload2[] = "http://example.com/getpolicy?id=456";
const char kTestDownload3[] = "http://example.com/getpolicy?id=789";
const char kTestPolicy[] =
    "{"
    "  \"Name\": {"
    "    \"Value\": \"disabled\""
    "  },"
    "  \"Second\": {"
    "    \"Value\": \"maybe\","
    "    \"Level\": \"Recommended\""
    "  }"
    "}";

class MockComponentCloudPolicyStoreDelegate
    : public ComponentCloudPolicyStore::Delegate {
 public:
  virtual ~MockComponentCloudPolicyStoreDelegate() {}

  MOCK_METHOD0(OnComponentCloudPolicyStoreUpdated, void());
};

}  // namespace

class ComponentCloudPolicyUpdaterTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_.reset(new ResourceCache(temp_dir_.path()));
    store_.reset(
        new ComponentCloudPolicyStore(&store_delegate_,
                                      cache_.get(),
                                      ComponentPolicyBuilder::kFakeUsername,
                                      ComponentPolicyBuilder::kFakeToken));
    fetcher_factory_.set_remove_fetcher_on_delete(true);
    scoped_refptr<net::URLRequestContextGetter> request_context;
    task_runner_ = new base::TestSimpleTaskRunner();
    updater_.reset(new ComponentCloudPolicyUpdater(
        task_runner_, request_context, store_.get()));
    ASSERT_FALSE(GetCurrentFetcher());
    ASSERT_TRUE(store_->policy().begin() == store_->policy().end());

    builder_.policy_data().set_policy_type(
        dm_protocol::kChromeExtensionPolicyType);
    builder_.policy_data().set_settings_entity_id(kTestExtension);
    builder_.payload().set_download_url(kTestDownload);
    builder_.payload().set_secure_hash(base::SHA1HashString(kTestPolicy));

    PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
    PolicyMap& policy = expected_bundle_.Get(ns);
    policy.Set("Name", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               base::Value::CreateStringValue("disabled"));
    policy.Set("Second", POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
               base::Value::CreateStringValue("maybe"));
  }

  net::TestURLFetcher* GetCurrentFetcher() {
    return fetcher_factory_.GetFetcherByID(0);
  }

  // Many tests also verify that a second fetcher is scheduled once the first
  // completes, either successfully or with a failure. This helper starts two
  // fetches immediately, so that the second is queued.
  void StartTwoFetches() {
    updater_->UpdateExternalPolicy(CreateResponse());

    builder_.payload().set_download_url(kTestDownload2);
    builder_.policy_data().set_settings_entity_id(kTestExtension2);
    updater_->UpdateExternalPolicy(CreateResponse());
  }

  scoped_ptr<em::PolicyFetchResponse> CreateResponse() {
    builder_.Build();
    return make_scoped_ptr(new em::PolicyFetchResponse(builder_.policy()));
  }

  std::string CreateSerializedResponse() {
    builder_.Build();
    return builder_.GetBlob();
  }

  base::ScopedTempDir temp_dir_;
  scoped_ptr<ResourceCache> cache_;
  scoped_ptr<ComponentCloudPolicyStore> store_;
  MockComponentCloudPolicyStoreDelegate store_delegate_;
  net::TestURLFetcherFactory fetcher_factory_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  scoped_ptr<ComponentCloudPolicyUpdater> updater_;
  ComponentPolicyBuilder builder_;
  PolicyBundle expected_bundle_;
};

TEST_F(ComponentCloudPolicyUpdaterTest, FetchAndCache) {
  StartTwoFetches();

  // The first fetch was scheduled; complete it.
  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());
  fetcher->set_response_code(200);
  fetcher->SetResponseString(kTestPolicy);
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // The second fetch was scheduled.
  fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());

  // The policy is being served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // No retries have been scheduled.
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(ComponentCloudPolicyUpdaterTest, LargeResponse) {
  std::string long_download("http://example.com/get?id=");
  long_download.append(20 * 1024, '1');
  builder_.payload().set_download_url(long_download);
  updater_->UpdateExternalPolicy(CreateResponse());

  builder_.policy_data().set_settings_entity_id(kTestExtension2);
  builder_.payload().set_download_url(kTestDownload2);
  updater_->UpdateExternalPolicy(CreateResponse());

  // The first request was dropped, and the second was started.
  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());

  // No retry is scheduled for the first request, because the policy was
  // rejected (too large).
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(ComponentCloudPolicyUpdaterTest, InvalidPolicy) {
  builder_.policy_data().set_username("wronguser@example.com");
  updater_->UpdateExternalPolicy(CreateResponse());

  builder_.policy_data().set_username(ComponentPolicyBuilder::kFakeUsername);
  builder_.policy_data().set_settings_entity_id(kTestExtension2);
  builder_.payload().set_download_url(kTestDownload2);
  updater_->UpdateExternalPolicy(CreateResponse());

  // The first request was dropped, and the second was started.
  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());

  // No retry is scheduled for the first request, because the policy was
  // rejected (invalid).
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(ComponentCloudPolicyUpdaterTest, AlreadyCached) {
  PolicyNamespace ns(POLICY_DOMAIN_EXTENSIONS, kTestExtension);
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  EXPECT_TRUE(store_->Store(ns,
                            CreateSerializedResponse(),
                            base::SHA1HashString(kTestPolicy),
                            kTestPolicy));
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // Seeing the same policy data again won't trigger a new fetch.
  updater_->UpdateExternalPolicy(CreateResponse());
  EXPECT_FALSE(GetCurrentFetcher());

  // And no retry is scheduled either.
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(ComponentCloudPolicyUpdaterTest, LargeFetch) {
  StartTwoFetches();

  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());
  fetcher->delegate()->OnURLFetchDownloadProgress(fetcher, 6 * 1024 * 1024, -1);

  // The second fetch was scheduled next.
  fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());

  // A retry is scheduled for the first fetcher.
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
}

TEST_F(ComponentCloudPolicyUpdaterTest, FetchFailed) {
  StartTwoFetches();

  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());
  fetcher->set_status(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                            net::ERR_NETWORK_CHANGED));
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // The second fetch was scheduled next.
  fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());

  // A retry is scheduled for the first fetcher.
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
}

TEST_F(ComponentCloudPolicyUpdaterTest, ServerFailed) {
  StartTwoFetches();

  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());
  fetcher->set_response_code(500);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // The second fetch was scheduled next.
  fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());

  // A retry is scheduled for the first fetcher.
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
}

TEST_F(ComponentCloudPolicyUpdaterTest, RetryLimit) {
  updater_->UpdateExternalPolicy(CreateResponse());

  // Failing due to client errors retries up to 3 times.
  for (int i = 0; i < 3; ++i) {
    net::TestURLFetcher* fetcher = GetCurrentFetcher();
    ASSERT_TRUE(fetcher);
    EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());

    // Make it fail with a 400 (bad request) client error.
    fetcher->set_response_code(400);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    // A retry is scheduled.
    EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
    // Make it retry immediately.
    task_runner_->RunPendingTasks();
    EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
  }

  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());

  // Make the last retry fail too; it won't retry anymore.
  fetcher->set_response_code(400);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(ComponentCloudPolicyUpdaterTest, RetryWithBackoff) {
  updater_->UpdateExternalPolicy(CreateResponse());

  base::TimeDelta expected_delay = base::TimeDelta::FromSeconds(60);
  const base::TimeDelta delay_cap = base::TimeDelta::FromHours(12);

  // The backoff delay is capped at 12 hours, which is reached after 10 retries:
  // 60 * 2^10 == 61440 > 43200 == 12 * 60 * 60
  for (int i = 0; i < 20; ++i) {
    net::TestURLFetcher* fetcher = GetCurrentFetcher();
    ASSERT_TRUE(fetcher);
    EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());

    // Make it fail with a 500 server error.
    fetcher->set_response_code(500);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    // A retry is scheduled. The delay is twice the last delay, with random
    // jitter from 80% to 100%.
    ASSERT_EQ(1u, task_runner_->GetPendingTasks().size());
    const base::TestPendingTask& task = task_runner_->GetPendingTasks().front();
    EXPECT_GT(task.delay,
              base::TimeDelta::FromMilliseconds(
                  0.799 * expected_delay.InMilliseconds()));
    EXPECT_LE(task.delay, expected_delay);

    if (i < 10) {
      // The delay cap hasn't been reached yet.
      EXPECT_LT(expected_delay, delay_cap);
      expected_delay *= 2;

      if (i == 9) {
        // The last doubling reached the cap.
        EXPECT_GT(expected_delay, delay_cap);
        expected_delay = delay_cap;
      }
    }

    // Make it retry immediately.
    task_runner_->RunPendingTasks();
    EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
  }
}

TEST_F(ComponentCloudPolicyUpdaterTest, DataValidationFails) {
  StartTwoFetches();

  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());
  fetcher->set_response_code(200);
  fetcher->SetResponseString("{ won't hash }");
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // The second fetch was scheduled next.
  fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());

  // A retry is scheduled for the first fetcher.
  EXPECT_EQ(1u, task_runner_->GetPendingTasks().size());
}

TEST_F(ComponentCloudPolicyUpdaterTest, FetchUpdatedData) {
  updater_->UpdateExternalPolicy(CreateResponse());

  // Same extension, but the download location has changed.
  builder_.payload().set_download_url(kTestDownload2);
  updater_->UpdateExternalPolicy(CreateResponse());

  // The first was cancelled and overridden by the second.
  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload2), fetcher->GetOriginalURL());

  // No retries are scheduled.
  EXPECT_TRUE(task_runner_->GetPendingTasks().empty());
}

TEST_F(ComponentCloudPolicyUpdaterTest, FetchUpdatedDataWithoutPolicy) {
  // Fetch the initial policy data.
  updater_->UpdateExternalPolicy(CreateResponse());

  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());
  fetcher->set_response_code(200);
  fetcher->SetResponseString(kTestPolicy);
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  Mock::VerifyAndClearExpectations(&store_delegate_);
  EXPECT_FALSE(GetCurrentFetcher());

  // The policy is being served.
  EXPECT_TRUE(store_->policy().Equals(expected_bundle_));

  // Same extension, but the download location has changed and is empty, meaning
  // that no policy should be served anymore.
  EXPECT_CALL(store_delegate_, OnComponentCloudPolicyStoreUpdated());
  builder_.payload().clear_download_url();
  builder_.payload().clear_secure_hash();
  updater_->UpdateExternalPolicy(CreateResponse());
  Mock::VerifyAndClearExpectations(&store_delegate_);

  // No fetcher was started for that.
  EXPECT_FALSE(GetCurrentFetcher());

  // And the policy has been removed.
  const PolicyBundle empty_bundle;
  EXPECT_TRUE(store_->policy().Equals(empty_bundle));
}

TEST_F(ComponentCloudPolicyUpdaterTest, InvalidatedJob) {
  StartTwoFetches();

  // Start a new fetch for the second extension with a new download URL; the
  // queued job is invalidated.
  builder_.payload().set_download_url(kTestDownload3);
  updater_->UpdateExternalPolicy(CreateResponse());

  // The first request is still pending.
  net::TestURLFetcher* fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload), fetcher->GetOriginalURL());

  // Make it fail.
  fetcher->set_response_code(500);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Now the second job was invalidated, and the third job (for the same
  // extension) is the next one.
  fetcher = GetCurrentFetcher();
  ASSERT_TRUE(fetcher);
  EXPECT_EQ(GURL(kTestDownload3), fetcher->GetOriginalURL());
}

}  // namespace policy
