// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud_policy_identity_strategy.h"
#include "chrome/browser/policy/logging_work_scheduler.h"
#include "chrome/browser/policy/mock_configuration_policy_store.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/testing_cloud_policy_subsystem.h"
#include "chrome/browser/policy/testing_policy_url_fetcher_factory.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_browser_process.h"
#include "chrome/test/testing_pref_service.h"
#include "content/common/url_fetcher.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

namespace em = enterprise_management;

class CloudPolicySubsystemTest;

namespace {

const char kGaiaAuthHeader[] = "GoogleLogin auth=secret123";
const char kDMAuthHeader[] = "GoogleDMToken token=token123456";
const char kDMToken[] = "token123456";

const char kDeviceManagementUrl[] =
    "http://localhost:12345/device_management_test";

// Constant responses of the identity strategy.
const char kMachineId[] = "dummy-cros-machine-123";
const char kMachineModel[] = "Pony";
const char kDeviceId[] = "abc-xyz-123";
const char kUsername[] = "john@smith.com";
const char kAuthToken[] = "secret123";
const char kPolicyType[] = "google/chrome/test";

}  // namespace

// A stripped-down identity strategy for the tests.
class TestingIdentityStrategy : public CloudPolicyIdentityStrategy {
 public:
  TestingIdentityStrategy() {
  }

  virtual std::string GetDeviceToken() OVERRIDE {
    return device_token_;
  }

  virtual std::string GetDeviceID() OVERRIDE {
    return kDeviceId;
  }

  virtual std::string GetMachineID() OVERRIDE {
    return kMachineId;
  }

  virtual std::string GetMachineModel() OVERRIDE {
    return kMachineModel;
  }

  virtual em::DeviceRegisterRequest_Type GetPolicyRegisterType() OVERRIDE {
    return em::DeviceRegisterRequest::USER;
  }

  virtual std::string GetPolicyType() OVERRIDE {
    return kPolicyType;
  }

  virtual bool GetCredentials(std::string* username,
                              std::string* auth_token) OVERRIDE {
    *username = kUsername;
    *auth_token = kAuthToken;;
    return !username->empty() && !auth_token->empty();
  }

  virtual void OnDeviceTokenAvailable(const std::string& token) OVERRIDE {
    device_token_ = token;
    NotifyDeviceTokenChanged();
  }

 private:
  std::string device_token_;

  DISALLOW_COPY_AND_ASSIGN(TestingIdentityStrategy);
};

// Tests CloudPolicySubsystem by intercepting its network requests.
// The requests are intercepted by PolicyRequestInterceptor and they are
// logged by LoggingWorkScheduler for further examination.
class CloudPolicySubsystemTest : public testing::Test {
 public:
  CloudPolicySubsystemTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        io_thread_(BrowserThread::IO, &loop_) {
  }

 protected:
  void StopMessageLoop() {
    loop_.QuitNow();
  }

  virtual void SetUp() {
    prefs_.reset(new TestingPrefService);
    CloudPolicySubsystem::RegisterPrefs(prefs_.get());
    ((TestingBrowserProcess*) g_browser_process)->SetLocalState(prefs_.get());

    logger_.reset(new EventLogger);
    factory_.reset(new TestingPolicyURLFetcherFactory(logger_.get()));
    URLFetcher::set_factory(factory_.get());
    identity_strategy_.reset(new TestingIdentityStrategy);
    ASSERT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
    cache_ = new UserPolicyCache(
        temp_user_data_dir_.path().AppendASCII("CloudPolicyControllerTest"));
    cloud_policy_subsystem_.reset(new TestingCloudPolicySubsystem(
        identity_strategy_.get(), cache_, kDeviceManagementUrl,
        logger_.get()));
    cloud_policy_subsystem_->CompleteInitialization(
        prefs::kDevicePolicyRefreshRate, 0);
  }

  virtual void TearDown() {
    ((TestingBrowserProcess*) g_browser_process)->SetLocalState(NULL);
    cloud_policy_subsystem_->Shutdown();
    cloud_policy_subsystem_.reset();
    factory_.reset();
    logger_.reset();
    prefs_.reset();
  }

  // Verifies for a given policy that it is provided by the subsystem.
  void VerifyPolicy(enum ConfigurationPolicyType type, Value* expected) {
    MockConfigurationPolicyStore store;
    EXPECT_CALL(store, Apply(_, _)).Times(AtLeast(1));
    cache_->GetManagedPolicyProvider()->Provide(&store);
    ASSERT_TRUE(Value::Equals(expected, store.Get(type)));
  }

  // Verifies that the last recorded run of the subsystem did not issue
  // too frequent requests:
  // - no more than 10 requests in the first 10 minutes
  // - no more then 12 requests per hour in the next 10 hours
  // TODO(gfeher): Thighten these conditions further. This will require
  // fine-tuning of the subsystem. See: http://crosbug.com/15195
  void VerifyServerLoad() {
    std::vector<int64> events;
    logger_->Swap(&events);

    ASSERT_FALSE(events.empty());

    int64 cur = 0;
    int count = 0;

    // Length and max number of requests for the first interval.
    int64 length = 10*60*1000; // 10 minutes
    int64 limit = 10; // maximum nr of requests in the first 10 minutes

    while (cur <= events.back()) {
      EXPECT_LE(EventLogger::CountEvents(events, cur, length), limit);
      count++;

      cur += length;

      // Length and max number of requests for the subsequent intervals.
      length = 60*60*1000; // 60 minutes
      limit = 12; // maxminum nr of requests in the next 60 minutes
    }

    EXPECT_GE(count, 11)
        << "No enough requests were fired during the test run.";
  }

  void ExecuteTest(const std::string& homepage_location) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // The first unexptected request of the policy subsystem will stop the
    // message loop.
    // This code relies on the fact that an InSequence object is active.
    EXPECT_CALL(*(factory_.get()), Intercept(_, _, _)).
       Times(AtMost(1)).
       WillOnce(InvokeWithoutArgs(
           this,
           &CloudPolicySubsystemTest::StopMessageLoop));
    loop_.RunAllPending();

    // Test conditions.
    EXPECT_EQ(CloudPolicySubsystem::SUCCESS, cloud_policy_subsystem_->state());
    StringValue homepage_value(homepage_location);
    VerifyPolicy(kPolicyHomepageLocation, &homepage_value);
    VerifyServerLoad();
  }

  scoped_ptr<TestingPolicyURLFetcherFactory> factory_;

 private:
  ScopedTempDir temp_user_data_dir_;

  MessageLoop loop_;
  BrowserThread ui_thread_;
  BrowserThread io_thread_;

  scoped_ptr<EventLogger> logger_;
  scoped_ptr<TestingIdentityStrategy> identity_strategy_;
  scoped_ptr<CloudPolicySubsystem> cloud_policy_subsystem_;
  scoped_ptr<PrefService> prefs_;
  CloudPolicyCacheBase* cache_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicySubsystemTest);
};

// An action that returns an URLRequestJob with an HTTP error code.
ACTION_P(CreateFailedResponse, http_error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  em::DeviceManagementResponse response_data;

  arg2->response_data = response_data.SerializeAsString();
  arg2->response_code = http_error_code;
}

// An action that returns an URLRequestJob with a successful device
// registration response.
ACTION_P(CreateSuccessfulRegisterResponse, token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  em::DeviceManagementResponse response_data;
  response_data.mutable_register_response()->set_device_management_token(token);

  arg2->response_data = response_data.SerializeAsString();
  arg2->response_code = 200;
}

// An action that returns an URLRequestJob with a successful policy response.
ACTION_P(CreateSuccessfulPolicyResponse, homepage_location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  em::CloudPolicySettings settings;
  settings.mutable_homepagelocation()->set_homepagelocation(homepage_location);
  em::PolicyData policy_data;
  policy_data.set_policy_type(kPolicyType);
  policy_data.set_policy_value(settings.SerializeAsString());

  em::DeviceManagementResponse response_data;
  em::DevicePolicyResponse* policy_response =
      response_data.mutable_policy_response();
  em::PolicyFetchResponse* fetch_response = policy_response->add_response();
  fetch_response->set_error_code(200);
  fetch_response->set_policy_data(policy_data.SerializeAsString());

  arg2->response_data = response_data.SerializeAsString();
  arg2->response_code = 200;
}

TEST_F(CloudPolicySubsystemTest, SuccessAfterSingleErrors) {
  InSequence c;

  EXPECT_CALL(*(factory_.get()), Intercept(kGaiaAuthHeader, "register", _)).
      WillOnce(CreateFailedResponse(901));
  EXPECT_CALL(*(factory_.get()), Intercept(kGaiaAuthHeader, "register", _)).
      WillOnce(CreateSuccessfulRegisterResponse(kDMToken));

  EXPECT_CALL(*(factory_.get()), Intercept(kDMAuthHeader, "policy", _)).
      WillOnce(CreateFailedResponse(501));
  EXPECT_CALL(*(factory_.get()), Intercept(kDMAuthHeader, "policy", _)).
      Times(200).
      WillRepeatedly(CreateSuccessfulPolicyResponse("http://www.google.com"));
  EXPECT_CALL(*(factory_.get()), Intercept(kDMAuthHeader, "policy", _)).
      WillOnce(CreateSuccessfulPolicyResponse("http://www.chromium.org"));

  ExecuteTest("http://www.chromium.org");
}

TEST_F(CloudPolicySubsystemTest, SuccessAfterRegistrationErrors) {
  InSequence c;

  EXPECT_CALL(*(factory_.get()), Intercept(kGaiaAuthHeader, "register", _)).
      Times(200).
      WillRepeatedly(CreateFailedResponse(901));
  EXPECT_CALL(*(factory_.get()), Intercept(kGaiaAuthHeader, "register", _)).
      WillOnce(CreateSuccessfulRegisterResponse(kDMToken));

  EXPECT_CALL(*(factory_.get()), Intercept(kDMAuthHeader, "policy", _)).
      WillOnce(CreateSuccessfulPolicyResponse("http://www.example.com"));

  ExecuteTest("http://www.example.com");
}

}  // policy
