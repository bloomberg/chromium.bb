// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/web_history_service.h"

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using history::WebHistoryService;
using ::testing::Return;

namespace {

// A testing web history service that does extra checks and creates a
// TestRequest instead of a normal request.
class TestingWebHistoryService : public WebHistoryService {
 public:
  explicit TestingWebHistoryService(
      ProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      const scoped_refptr<net::URLRequestContextGetter>& request_context)
      : WebHistoryService(token_service, signin_manager, request_context),
        expected_url_(GURL()),
        expected_audio_history_value_(false),
        current_expected_post_data_("") {}
  ~TestingWebHistoryService() override {}

  WebHistoryService::Request* CreateRequest(
      const GURL& url, const CompletionCallback& callback) override;

  // This is sorta an override but override and static don't mix.
  // This function just calls WebHistoryService::ReadResponse.
  static scoped_ptr<base::DictionaryValue> ReadResponse(
      Request* request);

  const std::string& GetExpectedPostData(WebHistoryService::Request* request);

  std::string GetExpectedAudioHistoryValue();

  void SetAudioHistoryCallback(bool success, bool new_enabled_value);

  void GetAudioHistoryCallback(bool success, bool new_enabled_value);

  void MultipleRequestsCallback(bool success, bool new_enabled_value);

  void SetExpectedURL(const GURL& expected_url) {
    expected_url_ = expected_url;
  }

  void SetExpectedAudioHistoryValue(bool expected_value) {
    expected_audio_history_value_ = expected_value;
  }

  void SetExpectedPostData(const std::string& expected_data) {
    current_expected_post_data_= expected_data;
  }

  void EnsureNoPendingRequestsRemain() {
    EXPECT_EQ(0u, GetNumberOfPendingAudioHistoryRequests());
  }

 private:
  GURL expected_url_;
  bool expected_audio_history_value_;
  std::string current_expected_post_data_;
  std::map<Request*, std::string> expected_post_data_;

  DISALLOW_COPY_AND_ASSIGN(TestingWebHistoryService);
};

// A testing request class that allows expected values to be filled in.
class TestRequest : public WebHistoryService::Request {
 public:
  TestRequest(const GURL& url,
              const WebHistoryService::CompletionCallback& callback,
              int response_code,
              const std::string& response_body)
      : web_history_service_(nullptr),
        url_(url),
        callback_(callback),
        response_code_(response_code),
        response_body_(response_body),
        post_data_(""),
        is_pending_(false) {
  }

  TestRequest(const GURL& url,
              const WebHistoryService::CompletionCallback& callback,
              TestingWebHistoryService* web_history_service)
      : web_history_service_(web_history_service),
        url_(url),
        callback_(callback),
        response_code_(net::HTTP_OK),
        response_body_(""),
        post_data_(""),
        is_pending_(false) {
    response_body_ = std::string("{\"history_recording_enabled\":") +
                     web_history_service->GetExpectedAudioHistoryValue() +
                     ("}");
  }

  ~TestRequest() override {}

  // history::Request overrides
  bool IsPending() override { return is_pending_; }
  int GetResponseCode() override { return response_code_; }
  const std::string& GetResponseBody() override { return response_body_; }
  void SetPostData(const std::string& post_data) override {
    post_data_ = post_data;
  }

  void Start() override {
    is_pending_ = true;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&TestRequest::MimicReturnFromFetch, base::Unretained(this)));
  }

  void MimicReturnFromFetch() {
    // Mimic a successful fetch and return. We don't actually send out a request
    // in unittests.
    EXPECT_EQ(web_history_service_->GetExpectedPostData(this), post_data_);
    callback_.Run(this, true);
  }

 private:
  TestingWebHistoryService* web_history_service_;
  GURL url_;
  WebHistoryService::CompletionCallback callback_;
  int response_code_;
  std::string response_body_;
  std::string post_data_;
  bool is_pending_;

  DISALLOW_COPY_AND_ASSIGN(TestRequest);
};

WebHistoryService::Request* TestingWebHistoryService::CreateRequest(
    const GURL& url, const CompletionCallback& callback) {
  EXPECT_EQ(expected_url_, url);
  WebHistoryService::Request* request =
      new TestRequest(url, callback, this);
  expected_post_data_[request] = current_expected_post_data_;
  return request;
}

scoped_ptr<base::DictionaryValue> TestingWebHistoryService::ReadResponse(
    Request* request) {
  return WebHistoryService::ReadResponse(request);
}

void TestingWebHistoryService::SetAudioHistoryCallback(
    bool success, bool new_enabled_value) {
  EXPECT_TRUE(success);
  // |new_enabled_value| should be equal to whatever the audio history value
  // was just set to.
  EXPECT_EQ(expected_audio_history_value_, new_enabled_value);
}

void TestingWebHistoryService::GetAudioHistoryCallback(
  bool success, bool new_enabled_value) {
  EXPECT_TRUE(success);
  EXPECT_EQ(expected_audio_history_value_, new_enabled_value);
}

void TestingWebHistoryService::MultipleRequestsCallback(
  bool success, bool new_enabled_value) {
  EXPECT_TRUE(success);
  EXPECT_EQ(expected_audio_history_value_, new_enabled_value);
}

const std::string& TestingWebHistoryService::GetExpectedPostData(
    Request* request) {
  return expected_post_data_[request];
}

std::string TestingWebHistoryService::GetExpectedAudioHistoryValue() {
  if (expected_audio_history_value_)
    return "true";
  return "false";
}

static KeyedService* BuildWebHistoryService(content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return new TestingWebHistoryService(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      SigninManagerFactory::GetForProfile(profile),
      profile->GetRequestContext());
}

}  // namespace

// A test class used for testing the WebHistoryService class.
// In order for WebHistoryService to be valid, we must have a valid
// ProfileSyncService. Using the ProfileSyncServiceMock class allows to
// assign specific return values as needed to make sure the web history
// service is available.
class WebHistoryServiceTest : public testing::Test {
 public:
  WebHistoryServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_DB_THREAD) {}
  ~WebHistoryServiceTest() override {}

  void SetUp() override {
    ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &ProfileSyncServiceMock::BuildMockProfileSyncService);
    // Use SetTestingFactoryAndUse to force creation and initialization.
    WebHistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &BuildWebHistoryService);

    ProfileSyncServiceMock* sync_service = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(&profile_));
    EXPECT_CALL(*sync_service,
                SyncActive()).WillRepeatedly(Return(true));
    syncer::ModelTypeSet result;
    result.Put(syncer::HISTORY_DELETE_DIRECTIVES);
    EXPECT_CALL(*sync_service,
                GetActiveDataTypes()).WillRepeatedly(Return(result));
  }
  void TearDown() override {
    base::RunLoop run_loop;
    base::MessageLoop::current()->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }
  Profile* profile() { return &profile_; }

  ProfileSyncServiceMock* mock_sync_service() {
    return static_cast<ProfileSyncServiceMock*>(
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(&profile_));
  }

 private:

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  DISALLOW_COPY_AND_ASSIGN(WebHistoryServiceTest);
};

TEST_F(WebHistoryServiceTest, GetAudioHistoryEnabled) {
  TestingWebHistoryService* web_history_service =
    static_cast<TestingWebHistoryService*>(
    WebHistoryServiceFactory::GetForProfile(profile()));
  EXPECT_TRUE(web_history_service);

  web_history_service->SetExpectedURL(
    GURL("https://history.google.com/history/api/lookup?client=audio"));
  web_history_service->SetExpectedAudioHistoryValue(true);
  web_history_service->GetAudioHistoryEnabled(
    base::Bind(&TestingWebHistoryService::GetAudioHistoryCallback,
    base::Unretained(web_history_service)));
  base::MessageLoop::current()->PostTask(
    FROM_HERE,
    base::Bind(&TestingWebHistoryService::EnsureNoPendingRequestsRemain,
    base::Unretained(web_history_service)));
}

TEST_F(WebHistoryServiceTest, SetAudioHistoryEnabledTrue) {
  TestingWebHistoryService* web_history_service =
      static_cast<TestingWebHistoryService*>(
          WebHistoryServiceFactory::GetForProfile(profile()));
  EXPECT_TRUE(web_history_service);

  web_history_service->SetExpectedURL(
      GURL("https://history.google.com/history/api/change"));
  web_history_service->SetExpectedAudioHistoryValue(true);
  web_history_service->SetExpectedPostData(
    "{\"client\":\"audio\",\"enable_history_recording\":true}");
  web_history_service->SetAudioHistoryEnabled(
      true,
      base::Bind(&TestingWebHistoryService::SetAudioHistoryCallback,
                 base::Unretained(web_history_service)));
  base::MessageLoop::current()->PostTask(
    FROM_HERE,
    base::Bind(&TestingWebHistoryService::EnsureNoPendingRequestsRemain,
               base::Unretained(web_history_service)));
}

TEST_F(WebHistoryServiceTest, SetAudioHistoryEnabledFalse) {
  TestingWebHistoryService* web_history_service =
    static_cast<TestingWebHistoryService*>(
    WebHistoryServiceFactory::GetForProfile(profile()));
  EXPECT_TRUE(web_history_service);

  web_history_service->SetExpectedURL(
    GURL("https://history.google.com/history/api/change"));
  web_history_service->SetExpectedAudioHistoryValue(false);
  web_history_service->SetExpectedPostData(
    "{\"client\":\"audio\",\"enable_history_recording\":false}");
  web_history_service->SetAudioHistoryEnabled(
    false,
    base::Bind(&TestingWebHistoryService::SetAudioHistoryCallback,
    base::Unretained(web_history_service)));
  base::MessageLoop::current()->PostTask(
    FROM_HERE,
    base::Bind(&TestingWebHistoryService::EnsureNoPendingRequestsRemain,
    base::Unretained(web_history_service)));
}

TEST_F(WebHistoryServiceTest, MultipleRequests) {
  TestingWebHistoryService* web_history_service =
    static_cast<TestingWebHistoryService*>(
    WebHistoryServiceFactory::GetForProfile(profile()));
  EXPECT_TRUE(web_history_service);

  web_history_service->SetExpectedURL(
    GURL("https://history.google.com/history/api/change"));
  web_history_service->SetExpectedAudioHistoryValue(false);
  web_history_service->SetExpectedPostData(
    "{\"client\":\"audio\",\"enable_history_recording\":false}");
  web_history_service->SetAudioHistoryEnabled(
    false,
    base::Bind(&TestingWebHistoryService::MultipleRequestsCallback,
    base::Unretained(web_history_service)));

  web_history_service->SetExpectedURL(
    GURL("https://history.google.com/history/api/lookup?client=audio"));
  web_history_service->SetExpectedPostData("");
  web_history_service->GetAudioHistoryEnabled(
    base::Bind(&TestingWebHistoryService::MultipleRequestsCallback,
    base::Unretained(web_history_service)));

  // Check that both requests are no longer pending.
  base::MessageLoop::current()->PostTask(
    FROM_HERE,
    base::Bind(&TestingWebHistoryService::EnsureNoPendingRequestsRemain,
    base::Unretained(web_history_service)));
}

TEST_F(WebHistoryServiceTest, VerifyReadResponse) {
  // Test that properly formatted response with good response code returns true
  // as expected.
  WebHistoryService::CompletionCallback completion_callback;
  scoped_ptr<WebHistoryService::Request> request(
      new TestRequest(
          GURL("http://history.google.com/"),
          completion_callback,
          net::HTTP_OK, /* response code */
          "{\n" /* response body */
          "  \"history_recording_enabled\": true\n"
          "}"));
  scoped_ptr<base::DictionaryValue> response_value;
  // ReadResponse deletes the request
  response_value = TestingWebHistoryService::ReadResponse(request.get());
  bool enabled_value = false;
  response_value->GetBoolean("history_recording_enabled", &enabled_value);
  EXPECT_TRUE(enabled_value);

  // Test that properly formatted response with good response code returns false
  // as expected.
  scoped_ptr<WebHistoryService::Request> request2(
      new TestRequest(
          GURL("http://history.google.com/"),
          completion_callback,
          net::HTTP_OK,
          "{\n"
          "  \"history_recording_enabled\": false\n"
          "}"));
  scoped_ptr<base::DictionaryValue> response_value2;
  // ReadResponse deletes the request
  response_value2 = TestingWebHistoryService::ReadResponse(request2.get());
  enabled_value = true;
  response_value2->GetBoolean("history_recording_enabled", &enabled_value);
  EXPECT_FALSE(enabled_value);

  // Test that a bad response code returns false.
  scoped_ptr<WebHistoryService::Request> request3(
      new TestRequest(
          GURL("http://history.google.com/"),
          completion_callback,
          net::HTTP_UNAUTHORIZED,
          "{\n"
          "  \"history_recording_enabled\": true\n"
          "}"));
  scoped_ptr<base::DictionaryValue> response_value3;
  // ReadResponse deletes the request
  response_value3 = TestingWebHistoryService::ReadResponse(request3.get());
  EXPECT_FALSE(response_value3);

  // Test that improperly formatted response returns false.
  // Note: we expect to see a warning when running this test similar to
  //   "Non-JSON response received from history server".
  // This test tests how that situation is handled.
  scoped_ptr<WebHistoryService::Request> request4(
      new TestRequest(
          GURL("http://history.google.com/"),
          completion_callback,
          net::HTTP_OK,
          "{\n"
          "  \"history_recording_enabled\": not true\n"
          "}"));
  scoped_ptr<base::DictionaryValue> response_value4;
  // ReadResponse deletes the request
  response_value4 = TestingWebHistoryService::ReadResponse(request4.get());
  EXPECT_FALSE(response_value4);

  // Test that improperly formatted response returns false.
  scoped_ptr<WebHistoryService::Request> request5(
      new TestRequest(
          GURL("http://history.google.com/"),
          completion_callback,
          net::HTTP_OK,
          "{\n"
          "  \"history_recording\": true\n"
          "}"));
  scoped_ptr<base::DictionaryValue> response_value5;
  // ReadResponse deletes the request
  response_value5 = TestingWebHistoryService::ReadResponse(request5.get());
  enabled_value = true;
  EXPECT_FALSE(response_value5->GetBoolean("history_recording_enabled",
                                           &enabled_value));
  EXPECT_TRUE(enabled_value);
}
