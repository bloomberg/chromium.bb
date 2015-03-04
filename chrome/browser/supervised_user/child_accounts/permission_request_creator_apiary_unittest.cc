// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAccountId[] = "account@gmail.com";

std::string BuildResponse() {
  base::DictionaryValue dict;
  base::DictionaryValue* permission_dict = new base::DictionaryValue;
  permission_dict->SetStringWithoutPathExpansion("id", "requestid");
  dict.SetWithoutPathExpansion("permissionRequest", permission_dict);
  std::string result;
  base::JSONWriter::Write(&dict, &result);
  return result;
}

}  // namespace

class PermissionRequestCreatorApiaryTest : public testing::Test {
 public:
  PermissionRequestCreatorApiaryTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())),
        permission_creator_(&token_service_,
                            kAccountId,
                            request_context_.get()) {
    token_service_.IssueRefreshTokenForUser(kAccountId, "refresh_token");
  }

 protected:
  void IssueAccessTokens() {
    token_service_.IssueAllTokensForAccount(
        kAccountId,
        "access_token",
        base::Time::Now() + base::TimeDelta::FromHours(1));
  }

  void IssueAccessTokenErrors() {
    token_service_.IssueErrorForAllPendingRequestsForAccount(
        kAccountId,
        GoogleServiceAuthError::FromServiceError("Error!"));
  }

  void CreateRequest(int url_fetcher_id, const GURL& url) {
    permission_creator_.set_url_fetcher_id_for_testing(url_fetcher_id);
    permission_creator_.CreateURLAccessRequest(
        url,
        base::Bind(&PermissionRequestCreatorApiaryTest::OnRequestCreated,
                   base::Unretained(this)));
  }

  net::TestURLFetcher* GetURLFetcher(int id) {
    net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(id);
    EXPECT_TRUE(url_fetcher);
    return url_fetcher;
  }

  void SendResponse(int url_fetcher_id,
                    net::URLRequestStatus::Status status,
                    const std::string& response) {
    net::TestURLFetcher* url_fetcher = GetURLFetcher(url_fetcher_id);
    url_fetcher->set_status(net::URLRequestStatus(status, 0));
    url_fetcher->set_response_code(net::HTTP_OK);
    url_fetcher->SetResponseString(response);
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void SendValidResponse(int url_fetcher_id) {
    SendResponse(url_fetcher_id,
                 net::URLRequestStatus::SUCCESS,
                 BuildResponse());
  }

  void SendFailedResponse(int url_fetcher_id) {
    SendResponse(url_fetcher_id,
                 net::URLRequestStatus::CANCELED,
                 std::string());
  }

  MOCK_METHOD1(OnRequestCreated, void(bool success));

  base::MessageLoop message_loop_;
  FakeProfileOAuth2TokenService token_service_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  PermissionRequestCreatorApiary permission_creator_;
};

TEST_F(PermissionRequestCreatorApiaryTest, Success) {
  CreateRequest(0, GURL("http://randomurl.com"));
  CreateRequest(1, GURL("http://anotherurl.com"));

  // We should have gotten a request for an access token.
  EXPECT_GT(token_service_.GetPendingRequests().size(), 0U);

  IssueAccessTokens();

  EXPECT_CALL(*this, OnRequestCreated(true));
  SendValidResponse(0);
  EXPECT_CALL(*this, OnRequestCreated(true));
  SendValidResponse(1);
}

TEST_F(PermissionRequestCreatorApiaryTest, AccessTokenError) {
  CreateRequest(0, GURL("http://randomurl.com"));

  // We should have gotten a request for an access token.
  EXPECT_EQ(1U, token_service_.GetPendingRequests().size());

  // Our callback should get called immediately on an error.
  EXPECT_CALL(*this, OnRequestCreated(false));
  IssueAccessTokenErrors();
}

TEST_F(PermissionRequestCreatorApiaryTest, NetworkError) {
  CreateRequest(0, GURL("http://randomurl.com"));

  // We should have gotten a request for an access token.
  EXPECT_EQ(1U, token_service_.GetPendingRequests().size());

  IssueAccessTokens();

  // Our callback should get called on an error.
  EXPECT_CALL(*this, OnRequestCreated(false));
  SendFailedResponse(0);
}
