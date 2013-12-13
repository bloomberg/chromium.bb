// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_simple_task_runner.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_header_io_helper.h"
#include "components/policy/core/common/cloud/policy_header_service.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
using enterprise_management::PolicyData;

namespace {
const char kDMServerURL[] = "http://server_url";
const char kPolicyHeaderName[] = "Chrome-Policy-Posture";
const char kExpectedPolicyHeader[] = "expected_header";

// Test version of the PolicyHeaderService that allows the tests to inject
// their own header values.
// TODO(atwilson): Remove this once PolicyHeaderService extracts the header
// directly from policy.
class TestPolicyHeaderService : public PolicyHeaderService {
 public:
  TestPolicyHeaderService(CloudPolicyStore* user_store,
                          CloudPolicyStore* device_store)
    : PolicyHeaderService(kDMServerURL, user_store, device_store) {
  }

  virtual ~TestPolicyHeaderService() {}

  void set_header(const std::string& header) { header_ = header; }

 protected:
  virtual std::string CreateHeaderValue() OVERRIDE { return header_; }

 private:
  std::string header_;
};

class TestCloudPolicyStore : public MockCloudPolicyStore {
 public:
  void SetPolicy(scoped_ptr<PolicyData> policy) {
    policy_ = policy.Pass();
    // Notify observers.
    NotifyStoreLoaded();
  }
};

class PolicyHeaderServiceTest : public testing::Test {
 public:
  PolicyHeaderServiceTest() {
    task_runner_ = make_scoped_refptr(new base::TestSimpleTaskRunner());
  }
  virtual ~PolicyHeaderServiceTest() {}

  virtual void SetUp() OVERRIDE {
    service_.reset(new TestPolicyHeaderService(&user_store_, &device_store_));
    service_->set_header(kExpectedPolicyHeader);
    helper_ = service_->CreatePolicyHeaderIOHelper(task_runner_).Pass();
  }

  virtual void TearDown() OVERRIDE {
    task_runner_->RunUntilIdle();
    // Helper should outlive the service.
    service_.reset();
    helper_.reset();
  }

  void ValidateHeader(const net::HttpRequestHeaders& headers,
                      bool should_exist) {
    if (should_exist) {
      std::string header;
      EXPECT_TRUE(headers.GetHeader(kPolicyHeaderName, &header));
      EXPECT_EQ(header, kExpectedPolicyHeader);
    } else {
      EXPECT_TRUE(headers.IsEmpty());
    }
  }

  base::MessageLoop loop_;
  scoped_ptr<TestPolicyHeaderService> service_;
  TestCloudPolicyStore user_store_;
  TestCloudPolicyStore device_store_;
  scoped_ptr<PolicyHeaderIOHelper> helper_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
};

}  // namespace

TEST_F(PolicyHeaderServiceTest, TestCreationAndShutdown) {
  // Just tests that the objects can be created and shutdown properly.
  EXPECT_TRUE(service_);
  EXPECT_TRUE(helper_);
}

TEST_F(PolicyHeaderServiceTest, TestWithAndWithoutPolicyHeader) {
  // Set policy - this should push a header to the PolicyHeaderIOHelper.
  scoped_ptr<PolicyData> policy(new PolicyData());
  user_store_.SetPolicy(policy.Pass());
  task_runner_->RunUntilIdle();

  net::TestURLRequestContext context;
  net::TestURLRequest request(
      GURL(kDMServerURL), net::DEFAULT_PRIORITY, NULL, &context);
  helper_->AddPolicyHeaders(&request);
  ValidateHeader(request.extra_request_headers(), true);

  // Now blow away the policy data.
  service_->set_header("");
  user_store_.SetPolicy(scoped_ptr<PolicyData>());
  task_runner_->RunUntilIdle();

  net::TestURLRequest request2(
      GURL(kDMServerURL), net::DEFAULT_PRIORITY, NULL, &context);
  helper_->AddPolicyHeaders(&request2);
  ValidateHeader(request2.extra_request_headers(), false);
}

}  // namespace policy
