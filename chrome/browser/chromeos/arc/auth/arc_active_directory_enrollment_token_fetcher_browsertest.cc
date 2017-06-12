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
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/auth/arc_active_directory_enrollment_token_fetcher.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_code_fetcher.h"
#include "chrome/browser/chromeos/arc/auth/arc_auth_service.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/cloud/test_request_interceptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

namespace em = enterprise_management;

namespace {

constexpr char kFakeDmToken[] = "fake-dm-token";
constexpr char kFakeEnrollmentToken[] = "fake-enrollment-token";
constexpr char kFakeUserId[] = "fake-user-id";
constexpr char kFakeAuthSessionId[] = "fake-auth-session-id";
constexpr char kFakeAdfsServerUrl[] = "http://example.com/adfs/ls/awesome.aspx";
constexpr char kFakeGuid[] = "f04557de-5da2-40ce-ae9d-b8874d8da96e";
constexpr char kNotYetFetched[] = "NOT-YET-FETCHED";
constexpr char kRedirectHeaderFormat[] =
    "HTTP/1.1 302 MOVED\n"
    "Location: %s\n"
    "\n";
constexpr char kBadRequestHeader[] = "HTTP/1.1 400 Bad Request";

// Returns a fake URL where the fake AD FS server redirects to
std::string GetDmServerRedirectUrl() {
  policy ::BrowserPolicyConnectorChromeOS* const connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->device_management_service()->GetServerUrl() +
         "/path/to/api?somedata=somevalue";
}

}  // namespace

namespace arc {

// Checks whether |request| is a valid request to enroll a play user and returns
// the corresponding protobuf.
em::ActiveDirectoryEnrollPlayUserRequest CheckRequestAndGetEnrollRequest(
    net::URLRequest* request) {
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

  // Check the DMToken.
  net::HttpRequestHeaders request_headers = request->extra_request_headers();
  std::string value;
  EXPECT_TRUE(request_headers.GetHeader("Authorization", &value));
  EXPECT_EQ("GoogleDMToken token=" + std::string(kFakeDmToken), value);

  // Extract the actual request proto.
  em::DeviceManagementRequest parsed_request;
  EXPECT_TRUE(parsed_request.ParseFromArray(bytes_reader->bytes(),
                                            bytes_reader->length()));
  EXPECT_TRUE(parsed_request.has_active_directory_enroll_play_user_request());

  return parsed_request.active_directory_enroll_play_user_request();
}

net::URLRequestJob* SendResponse(net::URLRequest* request,
                                 net::NetworkDelegate* network_delegate,
                                 const em::DeviceManagementResponse& response) {
  std::string response_data;
  EXPECT_TRUE(response.SerializeToString(&response_data));
  return new net::URLRequestTestJob(request, network_delegate,
                                    net::URLRequestTestJob::test_headers(),
                                    response_data, true);
}

// If this gets called, the test will fail.
net::URLRequestJob* NotReachedResponseJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  ADD_FAILURE() << "DMServer called when not expected";
  return nullptr;
}

// JobCallback for the interceptor to test non-SAML flow.
net::URLRequestJob* NonSamlResponseJob(net::URLRequest* request,
                                       net::NetworkDelegate* network_delegate) {
  CheckRequestAndGetEnrollRequest(request);

  // Response contains the enrollment token and user id.
  em::DeviceManagementResponse response;
  em::ActiveDirectoryEnrollPlayUserResponse* enroll_response =
      response.mutable_active_directory_enroll_play_user_response();
  enroll_response->set_enrollment_token(kFakeEnrollmentToken);
  enroll_response->set_user_id(kFakeUserId);

  return SendResponse(request, network_delegate, response);
}

// JobCallback for the interceptor to start the SAML flow.
net::URLRequestJob* InitiateSamlResponseJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  em::ActiveDirectoryEnrollPlayUserRequest enroll_request =
      CheckRequestAndGetEnrollRequest(request);

  EXPECT_FALSE(enroll_request.has_auth_session_id());

  // Response contains only SAML parameters to initialize the SAML flow.
  em::DeviceManagementResponse response;
  em::ActiveDirectoryEnrollPlayUserResponse* enroll_response =
      response.mutable_active_directory_enroll_play_user_response();
  em::SamlParametersProto* saml_parameters =
      enroll_response->mutable_saml_parameters();
  saml_parameters->set_auth_session_id(kFakeAuthSessionId);
  saml_parameters->set_auth_redirect_url(kFakeAdfsServerUrl);

  return SendResponse(request, network_delegate, response);
}

// JobCallback to redirect from ADFS server back to DM server.
net::URLRequestJob* RedirectResponseJob(
    const std::string& redirect_url,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  std::string redirect_header =
      base::StringPrintf(kRedirectHeaderFormat, redirect_url.c_str());
  return new net::URLRequestTestJob(request, network_delegate, redirect_header,
                                    std::string(), true);
}

// JobCallback returns a 400 Bad Request.
net::URLRequestJob* BadRequestResponseJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  return new net::URLRequestTestJob(request, network_delegate,
                                    kBadRequestHeader, std::string(), true);
}

// JobCallback that just returns HTTP 200 (DM server endpoint)
net::URLRequestJob* HttpOkResponseJob(net::URLRequest* request,
                                      net::NetworkDelegate* network_delegate) {
  return new net::URLRequestTestJob(request, network_delegate,
                                    net::URLRequestTestJob::test_headers(),
                                    std::string(), true);
}

// JobCallback for the interceptor to start the SAML flow.
net::URLRequestJob* FinishSamlResponseJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  DCHECK(request);
  em::ActiveDirectoryEnrollPlayUserRequest enroll_request =
      CheckRequestAndGetEnrollRequest(request);

  EXPECT_TRUE(enroll_request.has_auth_session_id());
  EXPECT_EQ(kFakeAuthSessionId, enroll_request.auth_session_id());

  // Response contains the enrollment token and user id.
  em::DeviceManagementResponse response;
  em::ActiveDirectoryEnrollPlayUserResponse* enroll_response =
      response.mutable_active_directory_enroll_play_user_response();
  enroll_response->set_enrollment_token(kFakeEnrollmentToken);
  enroll_response->set_user_id(kFakeUserId);

  return SendResponse(request, network_delegate, response);
}

// JobCallback that closes the current browser tab.
net::URLRequestJob* CloseTabJob(Browser* browser,
                                net::URLRequest* request,
                                net::NetworkDelegate* network_delegate) {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&TabStripModel::CloseSelectedTabs,
                     base::Unretained(browser->tab_strip_model())));

  return nullptr;
}

// JobCallback that closes the browser.
net::URLRequestJob* CloseBrowserJob(base::Closure close_closure,  // Haha!
                                    net::URLRequest* request,
                                    net::NetworkDelegate* network_delegate) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   close_closure);

  return nullptr;
}

class ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest
    : public InProcessBrowserTest {
 public:
  // Public wrapper, required for base::Bind.
  void CloseAllBrowsers_Wrapper() { CloseAllBrowsers(); }

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
    dm_interceptor_ = base::MakeUnique<policy::TestRequestInterceptor>(
        "localhost", content::BrowserThread::GetTaskRunnerForThread(
                         content::BrowserThread::IO));

    adfs_interceptor_ = base::MakeUnique<policy::TestRequestInterceptor>(
        "example.com", content::BrowserThread::GetTaskRunnerForThread(
                           content::BrowserThread::IO));

    token_fetcher_ =
        base::MakeUnique<ArcActiveDirectoryEnrollmentTokenFetcher>();

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

  // Stores a correct (fake) DM token. ArcActiveDirectoryEnrollmentTokenFetcher
  // will succeed to fetch the DM token.
  void StoreCorrectDmToken() {
    fake_cryptohome_client_->set_system_salt(
        chromeos::FakeCryptohomeClient::GetStubSystemSalt());
    fake_cryptohome_client_->SetServiceIsAvailable(true);
    // Store a fake DM token.
    base::RunLoop run_loop;
    auto dm_token_storage = base::MakeUnique<policy::DMTokenStorage>(
        g_browser_process->local_state());
    dm_token_storage->StoreDMToken(
        kFakeDmToken, base::BindOnce(
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

  // Does not store a correct DM token. ArcActiveDirectoryEnrollmentTokenFetcher
  // will fail to fetch the DM token.
  void FailDmToken() {
    fake_cryptohome_client_->set_system_salt(std::vector<uint8_t>());
    fake_cryptohome_client_->SetServiceIsAvailable(true);
  }

  void TearDownOnMainThread() override {
    user_manager_enabler_.reset();
    dm_interceptor_.reset();
    adfs_interceptor_.reset();
    testing_profile_.reset();
    chromeos::ProfileHelper::SetAlwaysReturnPrimaryUserForTesting(false);
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void FetchEnrollmentToken(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status* output_fetch_status,
      std::string* output_enrollment_token,
      std::string* output_user_id) {
    base::RunLoop run_loop;

    token_fetcher_->Fetch(base::Bind(
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

  void ExpectEnrollmentTokenFetchSucceeds() {
    std::string enrollment_token;
    std::string user_id;
    ArcActiveDirectoryEnrollmentTokenFetcher::Status fetch_status =
        ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE;

    FetchEnrollmentToken(&fetch_status, &enrollment_token, &user_id);

    EXPECT_EQ(ArcActiveDirectoryEnrollmentTokenFetcher::Status::SUCCESS,
              fetch_status);
    EXPECT_EQ(kFakeEnrollmentToken, enrollment_token);
    EXPECT_EQ(kFakeUserId, user_id);
  }

  void ExpectEnrollmentTokenFetchFails(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status expected_status) {
    CHECK_NE(expected_status,
             ArcActiveDirectoryEnrollmentTokenFetcher::Status::SUCCESS);

    // We expect enrollment_token is empty in this case. So initialize with
    // non-empty value.
    std::string enrollment_token = kNotYetFetched;
    std::string user_id = kNotYetFetched;
    ArcActiveDirectoryEnrollmentTokenFetcher::Status fetch_status =
        ArcActiveDirectoryEnrollmentTokenFetcher::Status::SUCCESS;

    FetchEnrollmentToken(&fetch_status, &enrollment_token, &user_id);

    EXPECT_EQ(expected_status, fetch_status);
    EXPECT_TRUE(enrollment_token.empty());
    EXPECT_TRUE(user_id.empty());
  }

  std::unique_ptr<policy::TestRequestInterceptor> dm_interceptor_;
  std::unique_ptr<policy::TestRequestInterceptor> adfs_interceptor_;

 private:
  std::unique_ptr<ArcActiveDirectoryEnrollmentTokenFetcher> token_fetcher_;
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  // DBusThreadManager owns this.
  chromeos::FakeCryptohomeClient* fake_cryptohome_client_;
  std::unique_ptr<TestingProfile> testing_profile_;

  DISALLOW_COPY_AND_ASSIGN(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest);
};

// Non-SAML flow fetches valid enrollment token and user id.
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       RequestAccountInfoSuccess) {
  dm_interceptor_->PushJobCallback(base::Bind(&NonSamlResponseJob));
  StoreCorrectDmToken();
  ExpectEnrollmentTokenFetchSucceeds();
}

// Failure to fetch DM token leads to token fetch failure.
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       DmTokenRetrievalFailed) {
  dm_interceptor_->PushJobCallback(base::Bind(NotReachedResponseJob));
  FailDmToken();
  ExpectEnrollmentTokenFetchFails(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE);
}

// Server responds with bad request and fails token fetch.
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       RequestAccountInfoError) {
  dm_interceptor_->PushJobCallback(
      policy::TestRequestInterceptor::BadRequestJob());
  StoreCorrectDmToken();
  ExpectEnrollmentTokenFetchFails(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE);
}

// ARC disabled leads to failed token fetch.
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       ArcDisabled) {
  dm_interceptor_->PushJobCallback(
      policy::TestRequestInterceptor::HttpErrorJob("904 ARC Disabled"));
  StoreCorrectDmToken();
  ExpectEnrollmentTokenFetchFails(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::ARC_DISABLED);
}

// Successful enrollment token fetch including SAML authentication.
// SAML flow works as follows:
//   1) Request to DM server responds with a redirect URL to the ADFS server.
//      ArcActiveDirectoryEnrollmentTokenFetcher opens a page with that URL.
//   2) ADFS authenticates and responds with a redirect back to DM server.
//   3) DM server acklowledges auth and responds with HTTP OK.
//   4) ArcActiveDirectoryEnrollmentTokenFetcher notices this, closes the page
//      and performs another request to DM server to fetch the token.
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       SamlFlowSuccess) {
  dm_interceptor_->PushJobCallback(base::Bind(&InitiateSamlResponseJob));
  adfs_interceptor_->PushJobCallback(
      base::Bind(&RedirectResponseJob, GetDmServerRedirectUrl()));
  dm_interceptor_->PushJobCallback(base::Bind(&HttpOkResponseJob));
  dm_interceptor_->PushJobCallback(base::Bind(&FinishSamlResponseJob));
  StoreCorrectDmToken();
  ExpectEnrollmentTokenFetchSucceeds();
}

// SAML flow fails since 3) responds with a bad request (see above).
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       SamlFlowFailsHttpError) {
  dm_interceptor_->PushJobCallback(base::Bind(&InitiateSamlResponseJob));
  adfs_interceptor_->PushJobCallback(
      base::Bind(&RedirectResponseJob, GetDmServerRedirectUrl()));
  dm_interceptor_->PushJobCallback(base::Bind(&BadRequestResponseJob));
  StoreCorrectDmToken();
  ExpectEnrollmentTokenFetchFails(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE);
}

// SAML flow fails since 3) responds with a network error.
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       SamlFlowFailsNetworkError) {
  dm_interceptor_->PushJobCallback(base::Bind(&InitiateSamlResponseJob));
  adfs_interceptor_->PushJobCallback(
      base::Bind(&RedirectResponseJob, GetDmServerRedirectUrl()));
  dm_interceptor_->PushJobCallback(
      policy::TestRequestInterceptor::ErrorJob(net::ERR_INVALID_RESPONSE));
  StoreCorrectDmToken();
  ExpectEnrollmentTokenFetchFails(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE);
}

// Failed SAML flow followed by a successful one.
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       SamlFlowSuccessAfterFailure) {
  StoreCorrectDmToken();

  // Failing flow.
  dm_interceptor_->PushJobCallback(base::Bind(&InitiateSamlResponseJob));
  adfs_interceptor_->PushJobCallback(
      base::Bind(&RedirectResponseJob, GetDmServerRedirectUrl()));
  dm_interceptor_->PushJobCallback(base::Bind(&BadRequestResponseJob));

  StoreCorrectDmToken();
  ExpectEnrollmentTokenFetchFails(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE);
  EXPECT_EQ(0u, dm_interceptor_->GetPendingSize());
  EXPECT_EQ(0u, adfs_interceptor_->GetPendingSize());

  // Need to close the tab. Otherwise, there's a race condition where the
  // existing tab makes a request to http://localhost/favicon.ico that is issued
  // at an unspecified time and can't be handled in dm_interceptor_
  // deterministically.
  browser()->tab_strip_model()->CloseSelectedTabs();

  // Successful flow.
  dm_interceptor_->PushJobCallback(base::Bind(&InitiateSamlResponseJob));
  adfs_interceptor_->PushJobCallback(
      base::Bind(&RedirectResponseJob, GetDmServerRedirectUrl()));
  dm_interceptor_->PushJobCallback(base::Bind(&HttpOkResponseJob));
  dm_interceptor_->PushJobCallback(base::Bind(&FinishSamlResponseJob));

  ExpectEnrollmentTokenFetchSucceeds();
}

// SAML flow fails if user closes the browser tab with the ADFS auth page.
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       SamlFlowFailsTabClosed) {
  dm_interceptor_->PushJobCallback(base::Bind(&InitiateSamlResponseJob));
  adfs_interceptor_->PushJobCallback(base::Bind(&CloseTabJob, browser()));
  StoreCorrectDmToken();
  ExpectEnrollmentTokenFetchFails(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE);
}

// SAML flow fails gracefully and doesn't blow up if user closes the whole
// browser with the ADFS auth page.
IN_PROC_BROWSER_TEST_F(ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest,
                       SamlFlowFailsBrowserClosed) {
  dm_interceptor_->PushJobCallback(base::Bind(&InitiateSamlResponseJob));
  adfs_interceptor_->PushJobCallback(base::Bind(
      &CloseBrowserJob,
      base::Bind(&ArcActiveDirectoryEnrollmentTokenFetcherBrowserTest::
                     CloseAllBrowsers_Wrapper,
                 base::Unretained(this))));
  StoreCorrectDmToken();
  ExpectEnrollmentTokenFetchFails(
      ArcActiveDirectoryEnrollmentTokenFetcher::Status::FAILURE);
}

}  // namespace arc
