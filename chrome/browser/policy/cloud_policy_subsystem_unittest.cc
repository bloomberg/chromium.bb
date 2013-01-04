// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/logging_work_scheduler.h"
#include "chrome/browser/policy/proto/cloud_policy.pb.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/testing_cloud_policy_subsystem.h"
#include "chrome/browser/policy/testing_policy_url_fetcher_factory.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using ::testing::AtMost;
using ::testing::InSequence;
using ::testing::_;
using content::BrowserThread;

namespace em = enterprise_management;

namespace {

const char kGaiaAuthHeader[] = "GoogleLogin auth=secret123";
const char kDMAuthHeader[] = "GoogleDMToken token=token123456";
const char kDMToken[] = "token123456";

const char kDeviceManagementUrl[] =
    "http://localhost:12345/device_management_test";

// Fake data to be included in requests.
const char kUsername[] = "john@smith.com";
const char kAuthToken[] = "secret123";
const char kPolicyType[] = "google/chrome/test";
const char kMachineId[] = "test-machine-id";

}  // namespace

// An action that returns an URLRequestJob with an HTTP error code.
ACTION_P(CreateFailedResponse, http_error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  em::DeviceManagementResponse response_data;

  arg3->response_data = response_data.SerializeAsString();
  arg3->response_code = http_error_code;
}

// An action that returns an URLRequestJob with a successful device
// registration response.
ACTION_P(CreateSuccessfulRegisterResponse, token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  em::DeviceManagementResponse response_data;
  response_data.mutable_register_response()->set_device_management_token(token);
  response_data.mutable_register_response()->set_enrollment_type(
      em::DeviceRegisterResponse::ENTERPRISE);

  arg3->response_data = response_data.SerializeAsString();
  arg3->response_code = 200;
}

// An action that returns an URLRequestJob with a successful policy response.
ACTION_P3(CreateSuccessfulPolicyResponse,
          homepage_location,
          set_serial_valid,
          serial_valid) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  em::CloudPolicySettings settings;
  settings.mutable_homepagelocation()->set_value(homepage_location);
  em::PolicyData policy_data;
  policy_data.set_policy_type(kPolicyType);
  policy_data.set_policy_value(settings.SerializeAsString());
  if (set_serial_valid)
    policy_data.set_valid_serial_number_missing(serial_valid);

  em::DeviceManagementResponse response_data;
  em::DevicePolicyResponse* policy_response =
      response_data.mutable_policy_response();
  em::PolicyFetchResponse* fetch_response = policy_response->add_response();
  fetch_response->set_error_code(200);
  fetch_response->set_policy_data(policy_data.SerializeAsString());

  arg3->response_data = response_data.SerializeAsString();
  arg3->response_code = 200;
}

// Tests CloudPolicySubsystem by intercepting its network requests.
// The requests are intercepted by PolicyRequestInterceptor and they are
// logged by LoggingWorkScheduler for further examination.
class CloudPolicySubsystemTestBase : public testing::Test {
 public:
  CloudPolicySubsystemTestBase()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_),
        io_thread_(BrowserThread::IO, &loop_) {}

  virtual ~CloudPolicySubsystemTestBase() {}

 protected:
  void StopMessageLoop() {
    loop_.QuitNow();
  }

  virtual void SetUp() {
    prefs_.reset(new TestingPrefServiceSimple);
    CloudPolicySubsystem::RegisterPrefs(prefs_.get());
    ((TestingBrowserProcess*) g_browser_process)->SetLocalState(prefs_.get());

    logger_.reset(new EventLogger);
    factory_.reset(new TestingPolicyURLFetcherFactory(logger_.get()));
    ASSERT_TRUE(temp_user_data_dir_.CreateUniqueTempDir());
    data_store_.reset(CloudPolicyDataStore::CreateForUserPolicies());
    cache_ = new UserPolicyCache(
        temp_user_data_dir_.path().AppendASCII("CloudPolicyControllerTest"),
        false  /* wait_for_policy_fetch */);
    cloud_policy_subsystem_.reset(new TestingCloudPolicySubsystem(
        data_store_.get(), cache_,
        kDeviceManagementUrl, logger_.get()));
    cloud_policy_subsystem_->CompleteInitialization(
        prefs::kDevicePolicyRefreshRate, 0);

    // Abort the test on unexpected requests.
    ON_CALL(factory(), Intercept(_, _, _, _))
       .WillByDefault(InvokeWithoutArgs(
           this,
           &CloudPolicySubsystemTestBase::StopMessageLoop));
  }

  virtual void TearDown() {
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
    cloud_policy_subsystem_->Shutdown();
    cloud_policy_subsystem_.reset();
    data_store_.reset();
    factory_.reset();
    logger_.reset();
    prefs_.reset();
  }

  void ExecuteTest() {
    // Stop the test once all the expectations are met. This relies on a
    // sequence being active (see tests below).
    EXPECT_CALL(factory(), Intercept(_, _, _, _))
        .Times(AtMost(1))
        .WillRepeatedly(
            InvokeWithoutArgs(this,
                              &CloudPolicySubsystemTestBase::StopMessageLoop));

    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    data_store_->set_user_name(kUsername);
    data_store_->SetGaiaToken(kAuthToken);
    data_store_->SetDeviceToken("", true);
    loop_.RunUntilIdle();
  }

  void VerifyTest(const std::string& homepage_location) {
    // Test conditions.
    EXPECT_EQ(CloudPolicySubsystem::SUCCESS, cloud_policy_subsystem_->state());
    StringValue homepage_value(homepage_location);
    VerifyPolicy(key::kHomepageLocation, &homepage_value);
    VerifyServerLoad();
  }

  void VerifyState(CloudPolicySubsystem::PolicySubsystemState state) {
    EXPECT_EQ(state, cloud_policy_subsystem_->state());
  }

  void ExpectSuccessfulRegistration() {
    EXPECT_CALL(factory(), Intercept(kGaiaAuthHeader, "register", _, _))
        .WillOnce(CreateSuccessfulRegisterResponse(kDMToken));
  }

  void ExpectFailedRegistration(int n, int code) {
    EXPECT_CALL(factory(), Intercept(kGaiaAuthHeader, "register", _, _))
        .Times(n)
        .WillRepeatedly(CreateFailedResponse(code));
  }

  void ExpectFailedPolicy(int n, int code) {
    EXPECT_CALL(factory(), Intercept(kDMAuthHeader, "policy", _, _))
        .Times(n)
        .WillRepeatedly(CreateFailedResponse(code));
  }

  void ExpectSuccessfulPolicy(int n,
                              const std::string& homepage) {
    EXPECT_CALL(factory(), Intercept(kDMAuthHeader, "policy", _, _))
        .Times(n)
        .WillRepeatedly(CreateSuccessfulPolicyResponse(homepage, false, false));
  }

  TestingPolicyURLFetcherFactory& factory() { return *factory_; }
  CloudPolicyDataStore* data_store() { return data_store_.get(); }

 private:
  // Verifies for a given policy that it is provided by the subsystem.
  void VerifyPolicy(const char* policy_name, Value* expected) {
    const PolicyMap* policy_map = cache_->policy();
    ASSERT_TRUE(Value::Equals(expected, policy_map->GetValue(policy_name)));
  }

  // Verifies that the last recorded run of the subsystem did not issue
  // too frequent requests:
  // - no more than 10 requests in the first 10 minutes
  // - no more then 12 requests per hour in the next 10 hours
  // TODO(gfeher): Thighten these conditions further. This will require
  // fine-tuning of the subsystem. See: http://crosbug.com/16637
  void VerifyServerLoad() {
    std::vector<int64> events;
    logger_->Swap(&events);

    ASSERT_FALSE(events.empty());

    int64 cur = 0;
    int count = 0;

    // Length and max number of requests for the first interval.
    int64 length = 10 * 60 * 1000; // 10 minutes
    int64 limit = 10; // maximum nr of requests in the first 10 minutes

    while (cur <= events.back()) {
      EXPECT_LE(EventLogger::CountEvents(events, cur, length), limit);
      count++;

      cur += length;

      // Length and max number of requests for the subsequent intervals.
      length = 60 * 60 * 1000; // 60 minutes
      limit = 12; // maxminum nr of requests in the next 60 minutes
    }

    EXPECT_GE(count, 11)
        << "No enough requests were fired during the test run.";
  }

  base::ScopedTempDir temp_user_data_dir_;

  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread io_thread_;

  scoped_ptr<EventLogger> logger_;
  scoped_ptr<CloudPolicyDataStore> data_store_;
  scoped_ptr<CloudPolicySubsystem> cloud_policy_subsystem_;
  scoped_ptr<PrefServiceSimple> prefs_;
  CloudPolicyCacheBase* cache_;

  scoped_ptr<TestingPolicyURLFetcherFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicySubsystemTestBase);
};

// A parameterized test case that simulates 100 failed registration attempts,
// then a successful one, then 100 failed policy fetch attempts and then 100
// successful policy fetches. The two parameters are the error codes for the
// failed registration and policy responses.

class CombinedTestDesc {
 public:
  CombinedTestDesc(int registration_error_code, int policy_error_code)
      : registration_error_code_(registration_error_code),
        policy_error_code_(policy_error_code) {
  }

  ~CombinedTestDesc() {}

  int registration_error_code() const { return registration_error_code_; }
  int policy_error_code() const { return policy_error_code_; }

 private:
  int registration_error_code_;
  int policy_error_code_;
};

class CloudPolicySubsystemCombinedTest
    : public CloudPolicySubsystemTestBase,
      public testing::WithParamInterface<CombinedTestDesc> {
};

TEST_P(CloudPolicySubsystemCombinedTest, Combined) {
  InSequence s;
  ExpectFailedRegistration(100, GetParam().registration_error_code());
  ExpectSuccessfulRegistration();
  ExpectFailedPolicy(100, GetParam().policy_error_code());
  ExpectSuccessfulPolicy(100, "http://www.google.com");
  ExpectSuccessfulPolicy(1, "http://www.chromium.org");
  ExecuteTest();
  VerifyTest("http://www.chromium.org");
}

// A random sample of error code pairs. Note that the following policy error
// codes (401, 403, 410) make the policy subsystem to try and reregister, and
// that is not expected in these tests.
INSTANTIATE_TEST_CASE_P(
    CloudPolicySubsystemCombinedTestInstance,
    CloudPolicySubsystemCombinedTest,
    testing::Values(
        CombinedTestDesc(403, 400),
        CombinedTestDesc(403, 404),
        CombinedTestDesc(403, 412),
        CombinedTestDesc(403, 500),
        CombinedTestDesc(403, 503),
        CombinedTestDesc(403, 902),
        CombinedTestDesc(902, 400),
        CombinedTestDesc(503, 404),
        CombinedTestDesc(500, 412),
        CombinedTestDesc(412, 500),
        CombinedTestDesc(404, 503),
        CombinedTestDesc(400, 902)));

// A parameterized test case that simulates 100 failed registration attempts,
// then a successful one, and then a succesful policy fetch. The parameter is
// the error code returned for registration attempts.

class CloudPolicySubsystemRegistrationTest
    : public CloudPolicySubsystemTestBase,
      public testing::WithParamInterface<int> {
};

TEST_P(CloudPolicySubsystemRegistrationTest, Registration) {
  InSequence s;
  ExpectFailedRegistration(100, GetParam());
  ExpectSuccessfulRegistration();
  ExpectSuccessfulPolicy(1, "http://www.youtube.com");
  ExecuteTest();
  VerifyTest("http://www.youtube.com");
}

INSTANTIATE_TEST_CASE_P(
    CloudPolicySubsystemRegistrationTestInstance,
    CloudPolicySubsystemRegistrationTest,
    // For the case of 401 see CloudPolicySubsystemRegistrationFailureTest
    testing::Values(400, 403, 404, 410, 412, 500, 503, 902));

// A test case that verifies that the subsystem understands the "not managed"
// response from the server.

class CloudPolicySubsystemRegistrationFailureTest
    : public CloudPolicySubsystemTestBase {
};

TEST_F(CloudPolicySubsystemRegistrationFailureTest, RegistrationFailure) {
  InSequence s;
  ExpectFailedRegistration(1, 401);
  ExecuteTest();
  VerifyState(CloudPolicySubsystem::BAD_GAIA_TOKEN);
}

// A parameterized test case that simulates a successful registration, then 100
// failed policy fetch attempts and then a successful one. The parameter is
// the error code returned for failed policy attempts.

class CloudPolicySubsystemPolicyTest
    : public CloudPolicySubsystemTestBase,
      public testing::WithParamInterface<int> {
};

TEST_P(CloudPolicySubsystemPolicyTest, Policy) {
  InSequence s;
  ExpectSuccessfulRegistration();
  ExpectFailedPolicy(100, GetParam());
  ExpectSuccessfulPolicy(1, "http://www.youtube.com");
  ExecuteTest();
  VerifyTest("http://www.youtube.com");
}

INSTANTIATE_TEST_CASE_P(
    CloudPolicySubsystemPolicyTestInstance,
    CloudPolicySubsystemPolicyTest,
    testing::Values(400, 404, 412, 500, 503, 902));

// A parameterized test case that simulates a successful registration, then 40
// failed policy fetch attempts and a successful registration after each of
// them. The parameter is the error code returned for registration attempts.

class CloudPolicySubsystemPolicyReregisterTest
    : public CloudPolicySubsystemTestBase,
      public testing::WithParamInterface<int> {
};

TEST_P(CloudPolicySubsystemPolicyReregisterTest, Policy) {
  // This logs a lot of WARNINGs. Temporarily increase the logging threshold.
  int prev_level = logging::GetMinLogLevel();
  logging::SetMinLogLevel(logging::LOG_ERROR);

  InSequence s;
  for (int i = 0; i < 40; i++) {
    ExpectSuccessfulRegistration();
    ExpectFailedPolicy(1, GetParam());
  }
  ExpectSuccessfulRegistration();
  ExpectSuccessfulPolicy(1, "http://www.youtube.com");
  ExecuteTest();
  VerifyTest("http://www.youtube.com");

  logging::SetMinLogLevel(prev_level);
}

INSTANTIATE_TEST_CASE_P(
    CloudPolicySubsystemPolicyReregisterTestInstance,
    CloudPolicySubsystemPolicyReregisterTest,
    testing::Values(401, 403, 410));

MATCHER_P(PolicyWithSerial, expected_serial, "") {
  return arg.policy_request().request(0).machine_id() == expected_serial;
}

class CloudPolicySubsystemSerialNumberRecoveryTest
    : public CloudPolicySubsystemTestBase {
 protected:
  virtual ~CloudPolicySubsystemSerialNumberRecoveryTest() {}

  virtual void SetUp() {
    CloudPolicySubsystemTestBase::SetUp();
    data_store()->set_machine_id(kMachineId);
  }

  void ExpectPolicyRequest(const std::string& serial,
                           bool set_serial_valid,
                           bool serial_valid) {
    EXPECT_CALL(factory(), Intercept(kDMAuthHeader, "policy",
                                     PolicyWithSerial(serial), _))
        .WillOnce(CreateSuccessfulPolicyResponse("", set_serial_valid,
                                                 serial_valid));
  }
};

// Tests that no serial is sent if the flag is not set.
TEST_F(CloudPolicySubsystemSerialNumberRecoveryTest, FlagNotSet) {
  InSequence s;
  ExpectSuccessfulRegistration();
  ExpectPolicyRequest("", false, false);
  ExpectPolicyRequest("", false, false);
  ExecuteTest();
}

// Tests that no serial is sent if the flag is set to false.
TEST_F(CloudPolicySubsystemSerialNumberRecoveryTest, FlagFalse) {
  InSequence s;
  ExpectSuccessfulRegistration();
  ExpectPolicyRequest("", true, false);
  ExpectPolicyRequest("", false, false);
  ExecuteTest();
}

// Tests that the serial is sent once if the server requests it.
TEST_F(CloudPolicySubsystemSerialNumberRecoveryTest, SerialRequested) {
  InSequence s;
  ExpectSuccessfulRegistration();
  ExpectPolicyRequest("", true, true);
  ExpectPolicyRequest(kMachineId, false, false);
  ExpectPolicyRequest("", false, false);
  ExecuteTest();
}

}  // policy
