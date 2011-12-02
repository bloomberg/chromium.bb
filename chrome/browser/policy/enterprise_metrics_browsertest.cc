// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/scoped_temp_dir.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/browser/policy/cloud_policy_controller.h"
#include "chrome/browser/policy/cloud_policy_data_store.h"
#include "chrome/browser/policy/device_management_backend.h"
#include "chrome/browser/policy/device_management_backend_mock.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/device_token_fetcher.h"
#include "chrome/browser/policy/enterprise_metrics.h"
#include "chrome/browser/policy/mock_cloud_policy_data_store.h"
#include "chrome/browser/policy/mock_device_management_service.h"
#include "chrome/browser/policy/policy_notifier.h"
#include "chrome/browser/policy/proto/device_management_backend.pb.h"
#include "chrome/browser/policy/proto/device_management_local.pb.h"
#include "chrome/browser/policy/user_policy_cache.h"
#include "chrome/browser/policy/user_policy_token_cache.h"
#include "content/test/test_browser_thread.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/mock_cryptohome_library.h"
#include "chrome/browser/chromeos/login/mock_signed_settings_helper.h"
#include "chrome/browser/chromeos/login/signed_settings.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/policy/device_policy_cache.h"
#include "chrome/browser/policy/enterprise_install_attributes.h"
#endif

namespace policy {

namespace em = enterprise_management;

using content::BrowserThread;
using testing::AnyNumber;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;
using testing::_;

namespace {

// Helper with common code for tests that use the DeviceManagementBackendImpl.
class DeviceManagementBackendTestHelper {
 public:
  explicit DeviceManagementBackendTestHelper(MessageLoop* loop) {
    service_.ScheduleInitialization(0);
    loop->RunAllPending();
    backend_.reset(service_.CreateBackendNotMocked());
  }

  DeviceManagementService& service() {
    return service_;
  }

  void ProcessRegisterRequest() {
    em::DeviceRegisterRequest request;
    DeviceRegisterResponseDelegateMock delegate;
    EXPECT_CALL(delegate, OnError(_)).Times(AnyNumber());
    EXPECT_CALL(delegate, HandleRegisterResponse(_)).Times(AnyNumber());
    backend_->ProcessRegisterRequest("gaia_auth_token", "oauth_token",
                                     "testid", request, &delegate);
  }

  void ProcessPolicyRequest() {
    em::DevicePolicyRequest request;
    DevicePolicyResponseDelegateMock delegate;
    EXPECT_CALL(delegate, OnError(_)).Times(AnyNumber());
    EXPECT_CALL(delegate, HandlePolicyResponse(_)).Times(AnyNumber());
    backend_->ProcessPolicyRequest("token", "testid",
                                   CloudPolicyDataStore::USER_AFFILIATION_NONE,
                                   request, &delegate);
  }

  void UnmockCreateBackend() {
    EXPECT_CALL(service_, CreateBackend())
        .WillOnce(Invoke(&service_,
                         &MockDeviceManagementService::CreateBackendNotMocked));
  }

  void ExpectStartJob(net::URLRequestStatus::Status status, int response_code,
                      const std::string& data) {
    net::ResponseCookies cookies;
    net::URLRequestStatus url_request_status(status, 0);
    EXPECT_CALL(service_, StartJob(_,_))
        .WillOnce(MockDeviceManagementServiceRespondToJob(
            url_request_status, response_code, cookies, data));
  }

  bool VerifyAndClearExpectations() {
    return Mock::VerifyAndClearExpectations(&service_);
  }

 private:
  MockDeviceManagementService service_;
  scoped_ptr<DeviceManagementBackend> backend_;

  DISALLOW_COPY_AND_ASSIGN(DeviceManagementBackendTestHelper);
};

}  // namespace

// This derives from testing::Test and not from InProcessBrowserTest and is
// linked with browser_tests. The reason is that these are just unit tests, but
// due to the static initialization of the UMA counters they have to run each
// in its own process to make sure the counters are not initialized before the
// test runs, and that the StatisticsRecorder instance created for the test
// doesn't interfere with counters in other tests.
class EnterpriseMetricsTest : public testing::Test {
 public:
  EnterpriseMetricsTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  void SetMetricName(const std::string& metric_name) {
    metric_name_ = metric_name;
    if (metric_name == kMetricToken) {
      expected_samples_.resize(kMetricTokenSize, 0);
    } else if (metric_name == kMetricPolicy) {
      expected_samples_.resize(kMetricPolicySize, 0);
    } else {
      NOTREACHED();
    }
  }

  virtual void SetUp() OVERRIDE {
#if defined(OS_CHROMEOS)
    // StatisticsProvider posts a task to FILE thread to read statistics
    // when the instance is created.
    chromeos::system::StatisticsProvider::GetInstance();
    // Run the FILE thread's message loop to process the task.
    file_thread_.DeprecatedGetThreadObject()->message_loop()->RunAllPending();
#endif
  }

  // Run pending tasks, and check that no unexpected samples were recorded.
  virtual void TearDown() OVERRIDE {
    RunAllPending();
    EXPECT_TRUE(CheckSamples());
  }

  const FilePath& temp_dir() {
    return temp_dir_.path();
  }

  MessageLoop* loop() {
    return &loop_;
  }

  void RunAllPending() {
    loop_.RunAllPending();
  }

  // Adds |sample| to the list of expected samples. Subsequent calls of
  // CheckSamples() will expect this sample.
  void ExpectSample(int sample) {
    ASSERT_FALSE(metric_name_.empty());
    ASSERT_GE(sample, 0);
    ASSERT_LT(sample, (int) expected_samples_.size());
    expected_samples_[sample]++;
  }

  // Checks the current recorded samples against the expected set. Returns
  // true if they match.
  bool CheckSamples() {
    EXPECT_FALSE(metric_name_.empty());
    if (metric_name_.empty())
      return false;
    bool expects_samples = false;
    for (size_t i = 0; i < expected_samples_.size(); ++i) {
      if (expected_samples_[i] > 0) {
        expects_samples = true;
        break;
      }
    }
    // The histogram won't be available until the first sample is measured.
    base::Histogram* histogram = NULL;
    bool found = base::StatisticsRecorder::FindHistogram(metric_name_,
                                                         &histogram);
    found = found && histogram != NULL;
    EXPECT_EQ(expects_samples, found);
    if (found != expects_samples)
      return false;
    if (histogram == NULL)
      return true;

    base::Histogram::SampleSet samples;
    histogram->SnapshotSample(&samples);

    bool result = true;
    int sum = 0;
    for (size_t i = 0; i < expected_samples_.size(); ++i) {
      EXPECT_EQ(expected_samples_[i], samples.counts(i))
          << "Mismatched sample index was " << i;
      if (expected_samples_[i] != samples.counts(i))
        result = false;
      sum += expected_samples_[i];
    }
    EXPECT_EQ(sum, samples.TotalCount());
    if (sum != samples.TotalCount())
      result = false;
    return result;
  }

 private:
  // All the UMA counters are registed at a static global singleton, which is
  // initialized on the constructor of base::StatisticsRecorder. This usually
  // lives in BrowserMain, but doesn't exist in unit tests, so we create it
  // here. The "staticness" of this class is also the reason to run each
  // test in a separate process.
  base::StatisticsRecorder statistics_recorder_;
  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  // |metric_name_| is the UMA counter that is being tested. It must be set
  // before any calls to ExpectSample or CheckSamples.
  std::string metric_name_;
  // List of expected samples.
  std::vector<int> expected_samples_;
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseMetricsTest);
};

TEST_F(EnterpriseMetricsTest, TokenFetchRequestFail) {
  SetMetricName(kMetricToken);
  DeviceManagementBackendTestHelper helper(loop());

  helper.ExpectStartJob(net::URLRequestStatus::FAILED, -1, std::string());
  ExpectSample(kMetricTokenFetchRequested);
  ExpectSample(kMetricTokenFetchRequestFailed);
  helper.ProcessRegisterRequest();
  EXPECT_TRUE(helper.VerifyAndClearExpectations());
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, TokenFetchInvalidData) {
  SetMetricName(kMetricToken);
  DeviceManagementBackendTestHelper helper(loop());

  helper.ExpectStartJob(net::URLRequestStatus::SUCCESS, 200, "\xff");
  ExpectSample(kMetricTokenFetchRequested);
  ExpectSample(kMetricTokenFetchBadResponse);
  helper.ProcessRegisterRequest();
  EXPECT_TRUE(helper.VerifyAndClearExpectations());
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, TokenFetchErrors) {
  SetMetricName(kMetricToken);
  DeviceManagementBackendTestHelper helper(loop());

  struct {
    int error_code;
    MetricToken sample;
  } cases[] = {
    { 400, kMetricTokenFetchRequestFailed },
    { 401, kMetricTokenFetchServerFailed },
    { 403, kMetricTokenFetchManagementNotSupported },
    { 404, kMetricTokenFetchServerFailed },
    { 405, kMetricTokenFetchInvalidSerialNumber },
    { 409, kMetricTokenFetchDeviceIdConflict },
    { 410, kMetricTokenFetchDeviceNotFound },
    { 412, kMetricTokenFetchServerFailed },
    { 500, kMetricTokenFetchServerFailed },
    { 503, kMetricTokenFetchServerFailed },
    { 902, kMetricTokenFetchServerFailed },
    { 491, kMetricTokenFetchServerFailed },
    { 901, kMetricTokenFetchDeviceNotFound },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    helper.ExpectStartJob(net::URLRequestStatus::SUCCESS,
                          cases[i].error_code, std::string());
    ExpectSample(kMetricTokenFetchRequested);
    ExpectSample(cases[i].sample);
    helper.ProcessRegisterRequest();
    EXPECT_TRUE(helper.VerifyAndClearExpectations());
    EXPECT_TRUE(CheckSamples());
  }
}

TEST_F(EnterpriseMetricsTest, TokenFetchReceiveResponse) {
  SetMetricName(kMetricToken);
  DeviceManagementBackendTestHelper helper(loop());

  helper.ExpectStartJob(net::URLRequestStatus::SUCCESS, 200, std::string());
  ExpectSample(kMetricTokenFetchRequested);
  ExpectSample(kMetricTokenFetchResponseReceived);
  helper.ProcessRegisterRequest();
  EXPECT_TRUE(helper.VerifyAndClearExpectations());
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, TokenFetchOK) {
  SetMetricName(kMetricToken);
  DeviceManagementBackendTestHelper helper(loop());

  // Test token fetcher.
  UserPolicyCache cache(temp_dir().AppendASCII("FetchTokenTest"), false);
  scoped_ptr<CloudPolicyDataStore> data_store;
  data_store.reset(CloudPolicyDataStore::CreateForUserPolicies());
  data_store->SetupForTesting("", "fake_device_id", "fake_user_name",
                              "fake_gaia_token", true);
  PolicyNotifier notifier;
  DeviceTokenFetcher fetcher(&helper.service(), &cache, data_store.get(),
                             &notifier);

  MockCloudPolicyDataStoreObserver observer;
  data_store->AddObserver(&observer);
  EXPECT_CALL(observer, OnDeviceTokenChanged()).Times(1);

  std::string data;
  em::DeviceManagementResponse response;
  response.mutable_register_response()->set_device_management_token("token");
  response.SerializeToString(&data);
  helper.ExpectStartJob(net::URLRequestStatus::SUCCESS, 200, data);

  // FetchToken() will reset the backend. Make sure it does the right thing.
  helper.UnmockCreateBackend();

  fetcher.FetchToken();

  ExpectSample(kMetricTokenFetchRequested);
  ExpectSample(kMetricTokenFetchResponseReceived);
  ExpectSample(kMetricTokenFetchOK);
  EXPECT_TRUE(helper.VerifyAndClearExpectations());
  EXPECT_TRUE(CheckSamples());

  // Cleanup.
  data_store->RemoveObserver(&observer);
}

TEST_F(EnterpriseMetricsTest, TokenStorage) {
  SetMetricName(kMetricToken);

  FilePath path = temp_dir().AppendASCII("StoreTokenTest");

  scoped_ptr<CloudPolicyDataStore> data_store;
  data_store.reset(CloudPolicyDataStore::CreateForUserPolicies());
  data_store->SetupForTesting("", "fake_device_id", "fake_user_name",
                              "fake_gaia_token", false);
  UserPolicyTokenCache cache(data_store.get(), path);

  // Try loading a non-existing file first.
  cache.Load();
  RunAllPending();
  // No samples expected.
  EXPECT_TRUE(CheckSamples());

  // Try loading an invalid file.
  std::string data("\xff");
  int result = file_util::WriteFile(path, data.c_str(), data.size());
  ASSERT_EQ((int) data.size(), result);

  // Make the data store expect a load from cache again.
  data_store->SetupForTesting("", "fake_device_id", "fake_user_name",
                              "fake_gaia_token", false);
  cache.Load();
  RunAllPending();
  ExpectSample(kMetricTokenLoadFailed);
  EXPECT_TRUE(CheckSamples());

  // Test storing a valid cache.
  data_store->SetupForTesting("token", "fake_device_id", "fake_user_name",
                              "fake_gaia_token", false);
  cache.OnDeviceTokenChanged();
  RunAllPending();
  ExpectSample(kMetricTokenStoreSucceeded);
  EXPECT_TRUE(CheckSamples());

  // Test loading a valid cache.
  // Make the data store expect a load from cache again.
  data_store->SetupForTesting("", "fake_device_id", "fake_user_name",
                              "fake_gaia_token", false);
  cache.Load();
  RunAllPending();
  ExpectSample(kMetricTokenLoadSucceeded);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, PolicyFetchRequestFails) {
  SetMetricName(kMetricPolicy);
  DeviceManagementBackendTestHelper helper(loop());

  helper.ExpectStartJob(net::URLRequestStatus::FAILED, -1, std::string());
  ExpectSample(kMetricPolicyFetchRequested);
  ExpectSample(kMetricPolicyFetchRequestFailed);
  helper.ProcessPolicyRequest();
  EXPECT_TRUE(helper.VerifyAndClearExpectations());
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, PolicyFetchInvalidData) {
  SetMetricName(kMetricPolicy);
  DeviceManagementBackendTestHelper helper(loop());

  helper.ExpectStartJob(net::URLRequestStatus::SUCCESS, 200, "\xff");
  ExpectSample(kMetricPolicyFetchRequested);
  ExpectSample(kMetricPolicyFetchBadResponse);
  helper.ProcessPolicyRequest();
  EXPECT_TRUE(helper.VerifyAndClearExpectations());
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, PolicyFetchErrors) {
  SetMetricName(kMetricPolicy);
  DeviceManagementBackendTestHelper helper(loop());

  struct {
    int error_code;
    MetricPolicy sample;
  } cases[] = {
    { 400, kMetricPolicyFetchRequestFailed },
    { 401, kMetricPolicyFetchInvalidToken },
    { 403, kMetricPolicyFetchServerFailed },
    { 404, kMetricPolicyFetchServerFailed },
    { 405, kMetricPolicyFetchServerFailed },
    { 409, kMetricPolicyFetchServerFailed },
    { 410, kMetricPolicyFetchServerFailed },
    { 412, kMetricPolicyFetchServerFailed },
    { 500, kMetricPolicyFetchServerFailed },
    { 503, kMetricPolicyFetchServerFailed },
    { 902, kMetricPolicyFetchNotFound },
    { 491, kMetricPolicyFetchServerFailed },
    { 901, kMetricPolicyFetchServerFailed },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    helper.ExpectStartJob(net::URLRequestStatus::SUCCESS, cases[i].error_code,
                          std::string());
    ExpectSample(kMetricPolicyFetchRequested);
    ExpectSample(cases[i].sample);
    helper.ProcessPolicyRequest();
    EXPECT_TRUE(helper.VerifyAndClearExpectations());
    EXPECT_TRUE(CheckSamples());
  }
}

TEST_F(EnterpriseMetricsTest, PolicyFetchReceiveResponse) {
  SetMetricName(kMetricPolicy);
  DeviceManagementBackendTestHelper helper(loop());

  helper.ExpectStartJob(net::URLRequestStatus::SUCCESS, 200, std::string());
  ExpectSample(kMetricPolicyFetchRequested);
  ExpectSample(kMetricPolicyFetchResponseReceived);
  helper.ProcessPolicyRequest();
  EXPECT_TRUE(helper.VerifyAndClearExpectations());
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, PolicyFetchInvalidPolicy) {
  SetMetricName(kMetricPolicy);

  UserPolicyCache cache(temp_dir().AppendASCII("UserPolicyCacheTest"), false);
  UserPolicyDiskCache::Delegate* cache_as_delegate =
      implicit_cast<UserPolicyDiskCache::Delegate*>(&cache);

  em::CachedCloudPolicyResponse response;
  response.mutable_cloud_policy()->set_policy_data("\xff");
  cache_as_delegate->OnDiskCacheLoaded(UserPolicyDiskCache::LOAD_RESULT_SUCCESS,
                                       response);
  ExpectSample(kMetricPolicyFetchInvalidPolicy);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, PolicyFetchTimestampInFuture) {
  SetMetricName(kMetricPolicy);

  UserPolicyCache cache(temp_dir().AppendASCII("UserPolicyCacheTest"), false);
  UserPolicyDiskCache::Delegate* cache_as_delegate =
      implicit_cast<UserPolicyDiskCache::Delegate*>(&cache);

  std::string data;
  em::PolicyData policy_data;
  base::TimeDelta timestamp =
      (base::Time::NowFromSystemTime() + base::TimeDelta::FromDays(1000)) -
      base::Time::UnixEpoch();
  policy_data.set_timestamp(timestamp.InMilliseconds());
  policy_data.SerializeToString(&data);
  em::CachedCloudPolicyResponse response;
  response.mutable_cloud_policy()->set_policy_data(data);
  cache_as_delegate->OnDiskCacheLoaded(UserPolicyDiskCache::LOAD_RESULT_SUCCESS,
                                       response);
  ExpectSample(kMetricPolicyFetchTimestampInFuture);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, PolicyFetchNotModified) {
  SetMetricName(kMetricPolicy);

  UserPolicyCache cache(temp_dir().AppendASCII("UserPolicyCacheTest"), false);
  UserPolicyDiskCache::Delegate* cache_as_delegate =
      implicit_cast<UserPolicyDiskCache::Delegate*>(&cache);

  std::string data;
  em::PolicyData policy_data;
  policy_data.set_timestamp(0);
  policy_data.SerializeToString(&data);
  em::CachedCloudPolicyResponse response;
  response.mutable_cloud_policy()->set_policy_data(data);
  cache_as_delegate->OnDiskCacheLoaded(UserPolicyDiskCache::LOAD_RESULT_SUCCESS,
                                       response);
  ExpectSample(kMetricPolicyFetchNotModified);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, PolicyFetchOK) {
  SetMetricName(kMetricPolicy);

  UserPolicyCache cache(temp_dir().AppendASCII("UserPolicyCacheTest"), false);

  std::string data;
  em::PolicyData policy_data;
  policy_data.set_timestamp(0);
  policy_data.SerializeToString(&data);
  em::CachedCloudPolicyResponse response;
  cache.SetPolicy(response.cloud_policy());
  ExpectSample(kMetricPolicyFetchOK);
  ExpectSample(kMetricPolicyFetchNotModified);
  EXPECT_TRUE(CheckSamples());
  // This also triggers a store. Update the expected samples.
  RunAllPending();
  ExpectSample(kMetricPolicyStoreSucceeded);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, PolicyFetchBadResponse) {
  SetMetricName(kMetricPolicy);

  UserPolicyCache cache(temp_dir().AppendASCII("UserPolicyCacheTest"), false);
  PolicyNotifier notifier;
  scoped_ptr<CloudPolicyDataStore> data_store;
  data_store.reset(CloudPolicyDataStore::CreateForUserPolicies());
  data_store->SetupForTesting("", "fake_device_id", "fake_user_name",
                              "fake_gaia_token", true);
  CloudPolicyController controller(NULL, &cache, NULL, data_store.get(),
                                   &notifier);
  em::DevicePolicyResponse device_policy_response;
  controller.HandlePolicyResponse(device_policy_response);
  ExpectSample(kMetricPolicyFetchBadResponse);
  EXPECT_TRUE(CheckSamples());

  // More bad responses.
  em::PolicyFetchResponse* policy_fetch_response =
      device_policy_response.add_response();
  policy_fetch_response->set_error_code(
      DeviceManagementBackend::kErrorServicePolicyNotFound);
  controller.HandlePolicyResponse(device_policy_response);
  ExpectSample(kMetricPolicyFetchBadResponse);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, PolicyStorage) {
  SetMetricName(kMetricPolicy);

  FilePath path = temp_dir().AppendASCII("UserPolicyDiskCacheTest");

  scoped_refptr<UserPolicyDiskCache> cache(
      new UserPolicyDiskCache(base::WeakPtr<UserPolicyDiskCache::Delegate>(),
                              path));

  // Load empty cache.
  cache->Load();
  RunAllPending();
  // No samples expected.
  EXPECT_TRUE(CheckSamples());

  // Load an invalid cache.
  std::string data("\xff");
  int result = file_util::WriteFile(path, data.c_str(), data.size());
  ASSERT_EQ((int) data.size(), result);

  cache->Load();
  RunAllPending();
  ExpectSample(kMetricPolicyLoadFailed);
  EXPECT_TRUE(CheckSamples());

  // Store a cache.
  em::CachedCloudPolicyResponse response;
  cache->Store(response);
  RunAllPending();
  ExpectSample(kMetricPolicyStoreSucceeded);
  EXPECT_TRUE(CheckSamples());

  // Load a good cache.
  cache->Load();
  RunAllPending();
  ExpectSample(kMetricPolicyLoadSucceeded);
  EXPECT_TRUE(CheckSamples());
}

#if defined(OS_CHROMEOS)

namespace {

// Wrapper around an instance of EnterpriseInstallAttributes that uses a
// mock CryptohomeLibrary. The wrapped EnterpriseInstallAttributes instance
// is expected to be used to load the attributes once.
class TestInstallAttributes {
 public:
  // |user| is the user to be returned as the value of the "enterprise.user"
  // attribute.
  explicit TestInstallAttributes(const std::string& user)
      : user_(user),
        owned_("true"),
        install_attributes_(&mock_cryptohome_library_) {}

  void ExpectUsage() {
    EXPECT_CALL(mock_cryptohome_library_, InstallAttributesIsReady())
        .WillOnce(Return(true));
    EXPECT_CALL(mock_cryptohome_library_, InstallAttributesIsInvalid())
        .WillOnce(Return(false));
    EXPECT_CALL(mock_cryptohome_library_, InstallAttributesIsFirstInstall())
        .WillOnce(Return(false));
    EXPECT_CALL(mock_cryptohome_library_,
                InstallAttributesGet("enterprise.owned", _))
        .WillOnce(DoAll(SetArgPointee<1>(owned_),
                        Return(true)));
    EXPECT_CALL(mock_cryptohome_library_,
                InstallAttributesGet("enterprise.user", _))
        .WillOnce(DoAll(SetArgPointee<1>(user_),
                        Return(true)));
  }

  EnterpriseInstallAttributes* install_attributes() {
    return &install_attributes_;
  }

 private:
  std::string user_;
  std::string owned_;
  chromeos::MockCryptohomeLibrary mock_cryptohome_library_;
  EnterpriseInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(TestInstallAttributes);
};

}  // namespace

// Helper for tests that use a DevicePolicyCache.
class DevicePolicyCacheTestHelper {
 public:
  DevicePolicyCacheTestHelper()
      : install_attributes_("user") {
    Init();
  }

  explicit DevicePolicyCacheTestHelper(const std::string& user)
      : install_attributes_(user) {
    Init();
  }

  void CompleteRetrieve(chromeos::SignedSettings::ReturnCode code) {
    device_policy_cache_->OnRetrievePolicyCompleted(code, response_);
  }

  void SetPolicy() {
    device_policy_cache_->SetPolicy(response_);
  }

  void SetData(const std::string& data) {
    response_.set_policy_data(data);
  }

  void SetGoodData() {
    em::PolicyData policy_data;
    policy_data.set_request_token("token");
    policy_data.set_username("user");
    policy_data.set_device_id("device_id");
    std::string data;
    policy_data.SerializeToString(&data);
    response_.set_policy_data(data);
  }

  void ExpectInstallAttributes() {
    install_attributes_.ExpectUsage();
  }

  void ExpectMockSignedSettings(chromeos::SignedSettings::ReturnCode code,
                                int expected_retrieves) {
    install_attributes_.ExpectUsage();
    EXPECT_CALL(mock_signed_settings_helper_, StartStorePolicyOp(_, _))
        .WillOnce(MockSignedSettingsHelperStorePolicy(code));
    EXPECT_CALL(mock_signed_settings_helper_,
                StartRetrievePolicyOp(_)).Times(expected_retrieves);
  }

  const em::PolicyFetchResponse& response() {
    return response_;
  }

 private:
  void Init() {
    data_store_.reset(CloudPolicyDataStore::CreateForUserPolicies());
    device_policy_cache_.reset(new DevicePolicyCache(data_store_.get(),
                               install_attributes_.install_attributes(),
                               &mock_signed_settings_helper_));
    data_store_->SetupForTesting("", "id", "user", "gaia_token", false);
  }

  chromeos::MockSignedSettingsHelper mock_signed_settings_helper_;
  scoped_ptr<CloudPolicyDataStore> data_store_;
  scoped_ptr<DevicePolicyCache> device_policy_cache_;
  em::PolicyFetchResponse response_;
  TestInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyCacheTestHelper);
};

TEST_F(EnterpriseMetricsTest, DevicePolicyNotFound) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;

  helper.CompleteRetrieve(chromeos::SignedSettings::NOT_FOUND);
  // No samples expected.
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyBadData) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;

  helper.SetData("\xff");
  helper.CompleteRetrieve(chromeos::SignedSettings::SUCCESS);
  ExpectSample(kMetricPolicyLoadFailed);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyMoreBadData) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;

  em::PolicyData policy_data;
  policy_data.set_request_token("token");
  std::string data;
  policy_data.SerializeToString(&data);
  helper.SetData(data);
  helper.CompleteRetrieve(chromeos::SignedSettings::SUCCESS);
  ExpectSample(kMetricPolicyLoadFailed);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyLoad) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;

  helper.SetGoodData();
  helper.CompleteRetrieve(chromeos::SignedSettings::SUCCESS);
  ExpectSample(kMetricPolicyLoadSucceeded);
  ExpectSample(kMetricPolicyFetchNotModified);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyStoreSuccess) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;
  helper.ExpectMockSignedSettings(chromeos::SignedSettings::SUCCESS, 1);
  helper.SetGoodData();
  helper.CompleteRetrieve(chromeos::SignedSettings::NOT_FOUND);
  helper.SetPolicy();
  ExpectSample(kMetricPolicyStoreSucceeded);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyStoreBadSignature) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;
  helper.ExpectMockSignedSettings(chromeos::SignedSettings::BAD_SIGNATURE, 0);
  helper.SetGoodData();
  helper.CompleteRetrieve(chromeos::SignedSettings::NOT_FOUND);
  helper.SetPolicy();
  ExpectSample(kMetricPolicyStoreFailed);
  ExpectSample(kMetricPolicyFetchBadSignature);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyStoreOperationFailed) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;
  helper.ExpectMockSignedSettings(chromeos::SignedSettings::OPERATION_FAILED,
                                  0);
  helper.SetGoodData();
  helper.CompleteRetrieve(chromeos::SignedSettings::NOT_FOUND);
  helper.SetPolicy();
  ExpectSample(kMetricPolicyStoreFailed);
  ExpectSample(kMetricPolicyFetchOtherFailed);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyNonEnterprise) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;

  scoped_ptr<CloudPolicyDataStore> data_store;
  data_store.reset(CloudPolicyDataStore::CreateForUserPolicies());
  EnterpriseInstallAttributes install_attributes(NULL);
  DevicePolicyCache device_policy_cache(data_store.get(), &install_attributes);

  data_store->SetupForTesting("", "id", "user", "gaia_token", false);
  helper.SetGoodData();
  device_policy_cache.OnRetrievePolicyCompleted(
      chromeos::SignedSettings::NOT_FOUND, helper.response());

  device_policy_cache.SetPolicy(helper.response());
  ExpectSample(kMetricPolicyFetchNonEnterpriseDevice);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyUserMismatch) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper("bogus");
  helper.ExpectInstallAttributes();
  helper.SetGoodData();
  helper.CompleteRetrieve(chromeos::SignedSettings::NOT_FOUND);
  helper.SetPolicy();
  ExpectSample(kMetricPolicyFetchUserMismatch);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyBadSignature) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;
  helper.CompleteRetrieve(chromeos::SignedSettings::NOT_FOUND);
  helper.CompleteRetrieve(chromeos::SignedSettings::BAD_SIGNATURE);
  ExpectSample(kMetricPolicyFetchBadSignature);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyOtherFailed) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;
  helper.CompleteRetrieve(chromeos::SignedSettings::NOT_FOUND);
  helper.CompleteRetrieve(chromeos::SignedSettings::OPERATION_FAILED);
  ExpectSample(kMetricPolicyFetchOtherFailed);
  EXPECT_TRUE(CheckSamples());
}

TEST_F(EnterpriseMetricsTest, DevicePolicyFetchInvalid) {
  SetMetricName(kMetricPolicy);
  DevicePolicyCacheTestHelper helper;

  helper.ExpectInstallAttributes();
  helper.CompleteRetrieve(chromeos::SignedSettings::NOT_FOUND);
  helper.SetData("\xff");
  helper.SetPolicy();
  ExpectSample(kMetricPolicyFetchInvalidPolicy);
  EXPECT_TRUE(CheckSamples());
}

#endif

}  // namespace policy
