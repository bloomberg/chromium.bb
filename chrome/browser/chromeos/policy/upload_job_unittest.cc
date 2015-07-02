// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/upload_job.h"

#include <queue>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/upload_job_impl.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kUploadPath[] = "/upload";
const char kRobotAccountId[] = "robot@gmail.com";
const char kCustomField1[] = "customfield1";
const char kCustomField2[] = "customfield2";
const char kTestPayload1[] = "**||--||PAYLOAD1||--||**";
const char kTestPayload2[] = "**||--||PAYLOAD2||--||**";
const char kTokenExpired[] = "EXPIRED_TOKEN";
const char kTokenInvalid[] = "INVALID_TOKEN";
const char kTokenValid[] = "VALID_TOKEN";

class RepeatingMimeBoundaryGenerator
    : public UploadJobImpl::MimeBoundaryGenerator {
 public:
  explicit RepeatingMimeBoundaryGenerator(char character)
      : character_(character) {}
  ~RepeatingMimeBoundaryGenerator() override {}

  // MimeBoundaryGenerator:
  std::string GenerateBoundary(size_t length) const override {
    return std::string(length, character_);
  }

 private:
  const char character_;

  DISALLOW_COPY_AND_ASSIGN(RepeatingMimeBoundaryGenerator);
};

class MockOAuth2TokenService : public FakeOAuth2TokenService {
 public:
  MockOAuth2TokenService();
  ~MockOAuth2TokenService() override;

  // OAuth2TokenService:
  void FetchOAuth2Token(RequestImpl* request,
                        const std::string& account_id,
                        net::URLRequestContextGetter* getter,
                        const std::string& client_id,
                        const std::string& client_secret,
                        const ScopeSet& scopes) override;

  // OAuth2TokenService:
  void InvalidateAccessTokenImpl(const std::string& account_id,
                                 const std::string& client_id,
                                 const ScopeSet& scopes,
                                 const std::string& access_token) override;

  void AddTokenToQueue(const std::string& token);
  bool IsTokenValid(const std::string& token) const;
  void SetTokenValid(const std::string& token);
  void SetTokenInvalid(const std::string& token);

 private:
  std::queue<std::string> token_replies_;
  std::set<std::string> valid_tokens_;

  DISALLOW_COPY_AND_ASSIGN(MockOAuth2TokenService);
};

MockOAuth2TokenService::MockOAuth2TokenService() {
}

MockOAuth2TokenService::~MockOAuth2TokenService() {
}

void MockOAuth2TokenService::FetchOAuth2Token(
    OAuth2TokenService::RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const FakeOAuth2TokenService::ScopeSet& scopes) {
  GoogleServiceAuthError response_error =
      GoogleServiceAuthError::AuthErrorNone();
  const base::Time response_expiration = base::Time::Now();
  std::string access_token;
  if (token_replies_.empty()) {
    response_error =
        GoogleServiceAuthError::FromServiceError("Service unavailable.");
  } else {
    access_token = token_replies_.front();
    token_replies_.pop();
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&OAuth2TokenService::RequestImpl::InformConsumer,
                            request->AsWeakPtr(), response_error, access_token,
                            response_expiration));
}

void MockOAuth2TokenService::AddTokenToQueue(const std::string& token) {
  token_replies_.push(token);
}

bool MockOAuth2TokenService::IsTokenValid(const std::string& token) const {
  return valid_tokens_.find(token) != valid_tokens_.end();
}

void MockOAuth2TokenService::SetTokenValid(const std::string& token) {
  valid_tokens_.insert(token);
}

void MockOAuth2TokenService::SetTokenInvalid(const std::string& token) {
  valid_tokens_.erase(token);
}

void MockOAuth2TokenService::InvalidateAccessTokenImpl(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  SetTokenInvalid(access_token);
}

}  // namespace

class UploadJobTestBase : public testing::Test, public UploadJob::Delegate {
 public:
  UploadJobTestBase()
      : test_browser_thread_bundle_(
            content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  // policy::UploadJob::Delegate:
  void OnSuccess() override {
    if (!expected_error_)
      run_loop_.Quit();
    else
      FAIL();
  }

  // policy::UploadJob::Delegate:
  void OnFailure(UploadJob::ErrorCode error_code) override {
    if (expected_error_ && *expected_error_.get() == error_code)
      run_loop_.Quit();
    else
      FAIL();
  }

  const GURL GetServerURL() const { return test_server_.GetURL(kUploadPath); }

  void SetExpectedError(scoped_ptr<UploadJob::ErrorCode> expected_error) {
    expected_error_ = expected_error.Pass();
  }

  // testing::Test:
  void SetUp() override {
    request_context_getter_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    oauth2_service_.AddAccount("robot@gmail.com");
    ASSERT_TRUE(test_server_.InitializeAndWaitUntilReady());
  }

  // testing::Test:
  void TearDown() override {
    ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
  }

 protected:
  scoped_ptr<UploadJob> PrepareUploadJob(scoped_ptr<
      UploadJobImpl::MimeBoundaryGenerator> mime_boundary_generator) {
    scoped_ptr<UploadJob> upload_job(new UploadJobImpl(
        GetServerURL(), kRobotAccountId, &oauth2_service_,
        request_context_getter_.get(), this, mime_boundary_generator.Pass()));

    std::map<std::string, std::string> header_entries;
    header_entries.insert(std::make_pair(kCustomField1, "CUSTOM1"));
    scoped_ptr<std::string> data(new std::string(kTestPayload1));
    upload_job->AddDataSegment("Name1", "file1.ext", header_entries,
                               data.Pass());

    header_entries.insert(std::make_pair(kCustomField2, "CUSTOM2"));
    scoped_ptr<std::string> data2(new std::string(kTestPayload2));
    upload_job->AddDataSegment("Name2", "", header_entries, data2.Pass());
    return upload_job.Pass();
  }

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  base::RunLoop run_loop_;
  net::test_server::EmbeddedTestServer test_server_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  MockOAuth2TokenService oauth2_service_;

  scoped_ptr<UploadJob::ErrorCode> expected_error_;
};

class UploadFlowTest : public UploadJobTestBase {
 public:
  UploadFlowTest() {}

  // UploadJobTestBase:
  void SetUp() override {
    UploadJobTestBase::SetUp();
    test_server_.RegisterRequestHandler(
        base::Bind(&UploadFlowTest::HandlePostRequest, base::Unretained(this)));
  }

  scoped_ptr<net::test_server::HttpResponse> HandlePostRequest(
      const net::test_server::HttpRequest& request) {
    EXPECT_TRUE(request.headers.find("Authorization") != request.headers.end());
    const std::string authorization_header =
        request.headers.at("Authorization");
    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    const size_t pos = authorization_header.find(" ");
    if (pos == std::string::npos) {
      response->set_code(net::HTTP_UNAUTHORIZED);
      return response.Pass();
    }

    const std::string token = authorization_header.substr(pos + 1);
    response->set_code(oauth2_service_.IsTokenValid(token)
                           ? net::HTTP_OK
                           : net::HTTP_UNAUTHORIZED);
    return response.Pass();
  }
};

TEST_F(UploadFlowTest, SuccessfulUpload) {
  oauth2_service_.SetTokenValid(kTokenValid);
  oauth2_service_.AddTokenToQueue(kTokenValid);
  scoped_ptr<UploadJob> upload_job = PrepareUploadJob(
      make_scoped_ptr(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
}

TEST_F(UploadFlowTest, TokenExpired) {
  oauth2_service_.SetTokenValid(kTokenValid);
  oauth2_service_.AddTokenToQueue(kTokenExpired);
  oauth2_service_.AddTokenToQueue(kTokenValid);
  scoped_ptr<UploadJob> upload_job = PrepareUploadJob(
      make_scoped_ptr(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
}

TEST_F(UploadFlowTest, TokenInvalid) {
  oauth2_service_.AddTokenToQueue(kTokenInvalid);
  oauth2_service_.AddTokenToQueue(kTokenInvalid);
  SetExpectedError(scoped_ptr<UploadJob::ErrorCode>(
      new UploadJob::ErrorCode(UploadJob::AUTHENTICATION_ERROR)));

  scoped_ptr<UploadJob> upload_job = PrepareUploadJob(
      make_scoped_ptr(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
}

TEST_F(UploadFlowTest, TokenFetchFailure) {
  SetExpectedError(scoped_ptr<UploadJob::ErrorCode>(
      new UploadJob::ErrorCode(UploadJob::AUTHENTICATION_ERROR)));

  scoped_ptr<UploadJob> upload_job = PrepareUploadJob(
      make_scoped_ptr(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
}

class UploadRequestTest : public UploadJobTestBase {
 public:
  UploadRequestTest() {}

  // UploadJobTestBase:
  void SetUp() override {
    UploadJobTestBase::SetUp();
    test_server_.RegisterRequestHandler(base::Bind(
        &UploadRequestTest::HandlePostRequest, base::Unretained(this)));
  }

  scoped_ptr<net::test_server::HttpResponse> HandlePostRequest(
      const net::test_server::HttpRequest& request) {
    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    EXPECT_EQ(expected_content_, request.content);
    return response.Pass();
  }

  void SetExpectedRequestContent(const std::string& expected_content) {
    expected_content_ = expected_content;
  }

 protected:
  std::string expected_content_;
};

TEST_F(UploadRequestTest, TestRequestStructure) {
  oauth2_service_.SetTokenValid(kTokenValid);
  oauth2_service_.AddTokenToQueue(kTokenValid);
  scoped_ptr<UploadJob> upload_job = PrepareUploadJob(
      make_scoped_ptr(new RepeatingMimeBoundaryGenerator('A')));
  SetExpectedRequestContent(
      "--AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"
      "Content-Disposition: form-data; "
      "name=\"Name1\"; filename=\"file1.ext\"\r\n"
      "customfield1: CUSTOM1\r\n"
      "\r\n"
      "**||--||PAYLOAD1||--||**\r\n"
      "--AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"
      "Content-Disposition: form-data; name=\"Name2\"\r\n"
      "customfield1: CUSTOM1\r\n"
      "customfield2: CUSTOM2\r\n"
      "\r\n"
      "**||--||PAYLOAD2||--||**\r\n--"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA--\r\n");

  upload_job->Start();
  run_loop_.Run();
}

}  // namespace policy
