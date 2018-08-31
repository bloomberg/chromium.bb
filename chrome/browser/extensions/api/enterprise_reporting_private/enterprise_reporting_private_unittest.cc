// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_api.h"

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace extensions {
namespace {

const char kFakeDMToken[] = "fake-dm-token";
const char kFakeClientId[] = "fake-client-id";
const char kFakeMachineNameReport[] = "{\"computername\":\"name\"}";

class MockCloudPolicyClient : public policy::MockCloudPolicyClient {
 public:
  explicit MockCloudPolicyClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : policy::MockCloudPolicyClient(std::move(url_loader_factory)) {}

  void UploadChromeDesktopReport(
      std::unique_ptr<enterprise_management::ChromeDesktopReportRequest>
          request,
      const StatusCallback& callback) override {
    UploadChromeDesktopReportProxy(request.get(), callback);
  }
  MOCK_METHOD2(UploadChromeDesktopReportProxy,
               void(enterprise_management::ChromeDesktopReportRequest*,
                    const StatusCallback&));

  void OnReportUploadedFailed(const StatusCallback& callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, false));
  }

  void OnReportUploadedSucceeded(const StatusCallback& callback) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, true));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCloudPolicyClient);
};

class FakeBrowserDMTokenStorage : public policy::BrowserDMTokenStorage {
 public:
  FakeBrowserDMTokenStorage() = default;
  ~FakeBrowserDMTokenStorage() override = default;

  void SetClientId(const std::string& client_id) { client_id_ = client_id; }

  // policy::BrowserDMTokenStorage:
  std::string InitClientId() override { return client_id_; }
  std::string InitEnrollmentToken() override { return std::string(); }
  std::string InitDMToken() override { return std::string(); }
  void SaveDMToken(const std::string& token) override {}

 private:
  std::string client_id_;

  DISALLOW_COPY_AND_ASSIGN(FakeBrowserDMTokenStorage);
};

}  // namespace

// Test for API enterprise.reportingPrivate.uploadChromeDesktopReport
class EnterpriseReportingPrivateUploadChromeDesktopReportTest
    : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateUploadChromeDesktopReportTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void TearDown() override { test_shared_loader_factory_->Detach(); }

  UIThreadExtensionFunction* CreateChromeDesktopReportingFunction(
      const std::string& dm_token) {
    EnterpriseReportingPrivateUploadChromeDesktopReportFunction* function =
        EnterpriseReportingPrivateUploadChromeDesktopReportFunction::
            CreateForTesting(test_shared_loader_factory_);
    auto client =
        std::make_unique<MockCloudPolicyClient>(test_shared_loader_factory_);
    client_ = client.get();
    function->SetCloudPolicyClientForTesting(std::move(client));
    function->SetRegistrationInfoForTesting(dm_token, kFakeClientId);
    return function;
  }

  std::string GenerateArgs(const char* name) {
    return base::StringPrintf("[{\"machineName\":%s}]", name);
  }

  std::string GenerateInvalidReport() {
    // This report is invalid as the chromeSignInUser dictionary should not be
    // wrapped in a list.
    return std::string(
        "[{\"browserReport\": "
        "{\"chromeUserProfileReport\":[{\"chromeSignInUser\":\"Name\"}]}}]");
  }

  MockCloudPolicyClient* client_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      test_shared_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(
      EnterpriseReportingPrivateUploadChromeDesktopReportTest);
};

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest,
       DeviceIsNotEnrolled) {
  ASSERT_EQ(enterprise_reporting::kDeviceNotEnrolled,
            RunFunctionAndReturnError(
                CreateChromeDesktopReportingFunction(std::string()),
                GenerateArgs(kFakeMachineNameReport)));
}

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest,
       ReportIsNotValid) {
  ASSERT_EQ(enterprise_reporting::kInvalidInputErrorMessage,
            RunFunctionAndReturnError(
                CreateChromeDesktopReportingFunction(kFakeDMToken),
                GenerateInvalidReport()));
}

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest, UploadFailed) {
  UIThreadExtensionFunction* function =
      CreateChromeDesktopReportingFunction(kFakeDMToken);
  EXPECT_CALL(*client_, SetupRegistration(kFakeDMToken, kFakeClientId, _))
      .Times(1);
  EXPECT_CALL(*client_, UploadChromeDesktopReportProxy(_, _))
      .WillOnce(WithArgs<1>(
          Invoke(client_, &MockCloudPolicyClient::OnReportUploadedFailed)));
  ASSERT_EQ(enterprise_reporting::kUploadFailed,
            RunFunctionAndReturnError(function,
                                      GenerateArgs(kFakeMachineNameReport)));
  ::testing::Mock::VerifyAndClearExpectations(client_);
}

TEST_F(EnterpriseReportingPrivateUploadChromeDesktopReportTest,
       UploadSucceeded) {
  UIThreadExtensionFunction* function =
      CreateChromeDesktopReportingFunction(kFakeDMToken);
  EXPECT_CALL(*client_, SetupRegistration(kFakeDMToken, kFakeClientId, _))
      .Times(1);
  EXPECT_CALL(*client_, UploadChromeDesktopReportProxy(_, _))
      .WillOnce(WithArgs<1>(
          Invoke(client_, &MockCloudPolicyClient::OnReportUploadedSucceeded)));
  ASSERT_EQ(nullptr, RunFunctionAndReturnValue(
                         function, GenerateArgs(kFakeMachineNameReport)));
  ::testing::Mock::VerifyAndClearExpectations(client_);
}

// Test for API enterprise.reportingPrivate.getDeviceId
class EnterpriseReportingPrivateGetDeviceIdTest : public ExtensionApiUnittest {
 public:
  EnterpriseReportingPrivateGetDeviceIdTest() {
    policy::BrowserDMTokenStorage::SetForTesting(&storage_);
  }

  void SetClientId(const std::string& client_id) {
    storage_.SetClientId(client_id);
  }

 private:
  FakeBrowserDMTokenStorage storage_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseReportingPrivateGetDeviceIdTest);
};

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, GetDeviceId) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId(kFakeClientId);
  std::unique_ptr<base::Value> id =
      RunFunctionAndReturnValue(function.get(), "[]");
  ASSERT_TRUE(id);
  ASSERT_TRUE(id->is_string());
  EXPECT_EQ(kFakeClientId, id->GetString());
}

TEST_F(EnterpriseReportingPrivateGetDeviceIdTest, DeviceIdNotExist) {
  auto function =
      base::MakeRefCounted<EnterpriseReportingPrivateGetDeviceIdFunction>();
  SetClientId("");
  ASSERT_EQ(enterprise_reporting::kDeviceIdNotFound,
            RunFunctionAndReturnError(function.get(), "[]"));
}

}  // namespace extensions
