// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/auth/arc_active_directory_enrollment_token_fetcher.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_code_fetcher.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/cloud/test_request_interceptor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "components/arc/arc_util.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_delegate.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeDmToken[] = "fake-dm-token";
constexpr char kFakeEnrollmentToken[] = "fake-enrollment-token";
constexpr char kFakeUserId[] = "fake-user-id";
constexpr char kFakeGuid[] = "f04557de-5da2-40ce-ae9d-b8874d8da96e";
constexpr char kNotYetFetched[] = "NOT-YET-FETCHED";

}  // namespace

namespace arc {

// JobCallback for the interceptor.
net::URLRequestJob* ResponseJob(net::URLRequest* request,
                                net::NetworkDelegate* network_delegate) {
  // Check the operation.
  std::string request_type;
  EXPECT_TRUE(
      net::GetValueForKeyInQuery(request->url(), "request", &request_type));
  EXPECT_EQ("active_directory_enroll_play_user", request_type);

  // Check content of request.
  const net::UploadDataStream* upload = request->get_upload();
  EXPECT_TRUE(upload);
  EXPECT_TRUE(upload->GetElementReaders());
  EXPECT_EQ(1u, upload->GetElementReaders()->size());
  EXPECT_TRUE((*upload->GetElementReaders())[0]->AsBytesReader());

  const net::UploadBytesElementReader* bytes_reader =
      (*upload->GetElementReaders())[0]->AsBytesReader();

  enterprise_management::DeviceManagementRequest parsed_request;
  EXPECT_TRUE(parsed_request.ParseFromArray(bytes_reader->bytes(),
                                            bytes_reader->length()));
  EXPECT_TRUE(parsed_request.has_active_directory_enroll_play_user_request());

  // Check the DMToken.
  net::HttpRequestHeaders request_headers = request->extra_request_headers();
  std::string value;
  EXPECT_TRUE(request_headers.GetHeader("Authorization", &value));
  EXPECT_EQ("GoogleDMToken token=" + std::string(kFakeDmToken), value);

  // Response contains the enrollment token and user id.
  enterprise_management::DeviceManagementResponse response;
  enterprise_management::ActiveDirectoryEnrollPlayUserResponse*
      enroll_response =
          response.mutable_active_directory_enroll_play_user_response();
  enroll_response->set_enrollment_token(kFakeEnrollmentToken);
  enroll_response->set_user_id(kFakeUserId);
  std::string response_data;
  EXPECT_TRUE(response.SerializeToString(&response_data));

  return new net::URLRequestTestJob(request, network_delegate,
                                    net::URLRequestTestJob::test_headers(),
                                    response_data, true);
}

class ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest
    : public InProcessBrowserTest {
 protected:
  ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest() = default;
  ~ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    "http://localhost");
    SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Set fake cryptohome, because we want to fail DMToken retrieval
    auto cryptohome_client = base::MakeUnique<chromeos::FakeCryptohomeClient>();
    fake_cryptohome_client_ = cryptohome_client.get();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetCryptohomeClient(
        std::move(cryptohome_client));
  }

  void SetUpOnMainThread() override {
    interceptor_ = base::MakeUnique<policy::TestRequestInterceptor>(
        "localhost", content::BrowserThread::GetTaskRunnerForThread(
                         content::BrowserThread::IO));

    user_manager_enabler_ =
        base::MakeUnique<chromeos::ScopedUserManagerEnabler>(
            new chromeos::FakeChromeUserManager());

    const AccountId account_id(AccountId::AdFromObjGuid(kFakeGuid));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(true);

    testing_profile_ = base::MakeUnique<TestingProfile>();
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        GetFakeUserManager()->GetPrimaryUser(), testing_profile_.get());
  }

  void StoreCorrectDmToken() {
    fake_cryptohome_client_->set_system_salt(
        chromeos::FakeCryptohomeClient::GetStubSystemSalt());
    fake_cryptohome_client_->SetServiceIsAvailable(true);
    // Store a fake DM token.
    base::RunLoop run_loop;
    auto dm_token_storage = base::MakeUnique<policy::DMTokenStorage>(
        g_browser_process->local_state());
    dm_token_storage->StoreDMToken(
        kFakeDmToken, base::Bind(
                          [](base::RunLoop* run_loop, bool success) {
                            EXPECT_TRUE(success);
                            run_loop->Quit();
                          },
                          &run_loop));
    // Because the StoreDMToken() operation interacts with the I/O thread,
    // RunUntilIdle() won't work here. Instead, use Run() and Quit() explicitly
    // in the callback.
    run_loop.Run();
  }

  void FailDmToken() {
    fake_cryptohome_client_->set_system_salt(std::vector<uint8_t>());
    fake_cryptohome_client_->SetServiceIsAvailable(true);
  }

  void TearDownOnMainThread() override {
    user_manager_enabler_.reset();
    interceptor_.reset();
    testing_profile_.reset();
    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  policy::TestRequestInterceptor* interceptor() { return interceptor_.get(); }

  static void FetchEnrollmentToken(
      ArcActiveDirectoryEnrollmentTokenFetcher* fetcher,
      ArcActiveDirectoryEnrollmentTokenFetcher::Status* output_fetch_status,
      std::string* output_enrollment_token,
      std::string* output_user_id) {
    base::RunLoop run_loop;

    fetcher->Fetch(base::Bind(
        [](ArcActiveDirectoryEnrollmentTokenFetcher::Status*
               output_fetch_status,
           std::string* output_enrollment_token, std::string* output_user_id,
           base::RunLoop* run_loop,
           ArcActiveDirectoryEnrollmentTokenFetcher::Status fetch_status,
           const std::string& enrollment_token, const std::string& user_id) {
          *output_fetch_status = fetch_status;
          *output_enrollment_token = enrollment_token;
          *output_user_id = user_id;
          run_loop->Quit();
        },
        output_fetch_status, output_enrollment_token, output_user_id,
        &run_loop));
    // Because the Fetch() operation needs to interact with other threads,
    // RunUntilIdle() won't work here. Instead, use Run() and Quit() explicitly
    // in the callback.
    run_loop.Run();
  }

 private:
  std::unique_ptr<policy::TestRequestInterceptor> interceptor_;
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  // DBusThreadManager owns this.
  chromeos::FakeCryptohomeClient* fake_cryptohome_client_;
  std::unique_ptr<TestingProfile> testing_profile_;

  DISALLOW_COPY_AND_ASSIGN(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       RequestAccountInfoSuccess) {
  interceptor()->PushJobCallback(base::Bind(&ResponseJob));

  // Retrieving the DM token will succeed.
  StoreCorrectDmToken();

  std::string enrollment_token;
  std::string user_id;
  ArcActiveDirectoryEnrollmentTokenFetcher::Status fetch_status =
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE;

  auto token_fetcher =
      base::MakeUnique<ArcActiveDirectoryEnrollmentTokenFetcher>();
  FetchEnrollmentToken(token_fetcher.get(), &fetch_status, &enrollment_token,
                       &user_id);

  EXPECT_EQ(ArcActiveDirectoryEnrollmentTokenFetcher::Status::SUCCESS,
            fetch_status);
  EXPECT_EQ(kFakeEnrollmentToken, enrollment_token);
  EXPECT_EQ(kFakeUserId, user_id);
}

IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       DmTokenRetrievalFailed) {
  interceptor()->PushJobCallback(base::Bind(
      [](net::URLRequest*, net::NetworkDelegate*) -> net::URLRequestJob* {
        ADD_FAILURE() << "DMServer called when not expected";
        return nullptr;
      }));

  // Retrieving the DM token will fail.
  FailDmToken();

  // We expect enrollment_token is empty in this case. So initialize with
  // non-empty value.
  std::string enrollment_token = kNotYetFetched;
  std::string user_id = kNotYetFetched;
  ArcActiveDirectoryEnrollmentTokenFetcher::Status fetch_status =
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::SUCCESS;

  auto token_fetcher =
      base::MakeUnique<ArcActiveDirectoryEnrollmentTokenFetcher>();
  FetchEnrollmentToken(token_fetcher.get(), &fetch_status, &enrollment_token,
                       &user_id);

  EXPECT_EQ(ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE,
            fetch_status);
  EXPECT_EQ(std::string(), enrollment_token);
  EXPECT_EQ(std::string(), user_id);
}

IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       RequestAccountInfoError) {
  interceptor()->PushJobCallback(
      policy::TestRequestInterceptor::BadRequestJob());

  // Retrieving the DM token will succeed.
  StoreCorrectDmToken();

  // We expect enrollment_token is empty in this case. So initialize with
  // non-empty value.
  std::string enrollment_token = kNotYetFetched;
  std::string user_id = kNotYetFetched;
  ArcActiveDirectoryEnrollmentTokenFetcher::Status fetch_status =
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::SUCCESS;

  auto token_fetcher =
      base::MakeUnique<ArcActiveDirectoryEnrollmentTokenFetcher>();
  FetchEnrollmentToken(token_fetcher.get(), &fetch_status, &enrollment_token,
                       &user_id);

  EXPECT_EQ(ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE,
            fetch_status);
  EXPECT_EQ(std::string(), enrollment_token);
  EXPECT_EQ(std::string(), user_id);
}

IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       ArcDisabled) {
  interceptor()->PushJobCallback(
      policy::TestRequestInterceptor::HttpErrorJob("904 Arc Disabled"));

  // Retrieving the DM token will succeed.
  StoreCorrectDmToken();

  // We expect enrollment_token is empty in this case. So initialize with
  // non-empty value.
  std::string enrollment_token = kNotYetFetched;
  std::string user_id = kNotYetFetched;
  ArcActiveDirectoryEnrollmentTokenFetcher::Status fetch_status =
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::SUCCESS;

  auto token_fetcher =
      base::MakeUnique<ArcActiveDirectoryEnrollmentTokenFetcher>();
  FetchEnrollmentToken(token_fetcher.get(), &fetch_status, &enrollment_token,
                       &user_id);

  EXPECT_EQ(ArcActiveDirectoryEnrollmentTokenFetcher::Status::ARC_DISABLED,
            fetch_status);
  EXPECT_EQ(std::string(), enrollment_token);
  EXPECT_EQ(std::string(), user_id);
}

}  // namespace arc
