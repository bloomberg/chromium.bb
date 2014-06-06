// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
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
    service_.reset(new PolicyHeaderService(kDMServerURL,
                                           kPolicyVerificationKeyHash,
                                           &user_store_,
                                           &device_store_));
    helper_ = service_->CreatePolicyHeaderIOHelper(task_runner_).Pass();
  }

  virtual void TearDown() OVERRIDE {
    task_runner_->RunUntilIdle();
    // Helper should outlive the service.
    service_.reset();
    helper_.reset();
  }

  void ValidateHeader(const net::HttpRequestHeaders& headers,
                      const std::string& expected_dmtoken,
                      const std::string& expected_policy_token) {
    if (expected_dmtoken.empty()) {
      EXPECT_TRUE(headers.IsEmpty());
    } else {
      // Read header.
      std::string header;
      EXPECT_TRUE(headers.GetHeader(kPolicyHeaderName, &header));
      // Decode the base64 value into JSON.
      std::string decoded;
      base::Base64Decode(header, &decoded);
      // Parse the JSON.
      scoped_ptr<base::Value> value(base::JSONReader::Read(decoded));
      ASSERT_TRUE(value);
      base::DictionaryValue* dict;
      EXPECT_TRUE(value->GetAsDictionary(&dict));
      // Read the values and verify them vs the expected values.
      std::string dm_token;
      dict->GetString("user_dmtoken", &dm_token);
      EXPECT_EQ(dm_token, expected_dmtoken);
      std::string policy_token;
      dict->GetString("user_policy_token", &policy_token);
      EXPECT_EQ(policy_token, expected_policy_token);
    }
  }

  base::MessageLoop loop_;
  scoped_ptr<PolicyHeaderService> service_;
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
  std::string expected_dmtoken = "expected_dmtoken";
  std::string expected_policy_token = "expected_dmtoken";
  policy->set_request_token(expected_dmtoken);
  policy->set_policy_token(expected_policy_token);
  user_store_.SetPolicy(policy.Pass());
  task_runner_->RunUntilIdle();

  net::TestURLRequestContext context;
  net::TestURLRequest request(
      GURL(kDMServerURL), net::DEFAULT_PRIORITY, NULL, &context);
  helper_->AddPolicyHeaders(request.url(), &request);
  ValidateHeader(request.extra_request_headers(), expected_dmtoken,
                 expected_policy_token);

  // Now blow away the policy data.
  user_store_.SetPolicy(scoped_ptr<PolicyData>());
  task_runner_->RunUntilIdle();

  net::TestURLRequest request2(
      GURL(kDMServerURL), net::DEFAULT_PRIORITY, NULL, &context);
  helper_->AddPolicyHeaders(request2.url(), &request2);
  ValidateHeader(request2.extra_request_headers(), "", "");
}

}  // namespace policy
