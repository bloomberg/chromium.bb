// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

using bookmarks_helper::AddURL;

const char kShortLivedOAuth2Token[] =
    "{"
    "  \"refresh_token\": \"short_lived_refresh_token\","
    "  \"access_token\": \"short_lived_access_token\","
    "  \"expires_in\": 5,"  // 5 seconds.
    "  \"token_type\": \"Bearer\""
    "}";

const char kValidOAuth2Token[] = "{"
                                 "  \"refresh_token\": \"new_refresh_token\","
                                 "  \"access_token\": \"new_access_token\","
                                 "  \"expires_in\": 3600,"  // 1 hour.
                                 "  \"token_type\": \"Bearer\""
                                 "}";

const char kInvalidGrantOAuth2Token[] = "{"
                                        "  \"error\": \"invalid_grant\""
                                        "}";

const char kInvalidClientOAuth2Token[] = "{"
                                         "  \"error\": \"invalid_client\""
                                         "}";

const char kEmptyOAuth2Token[] = "";

const char kMalformedOAuth2Token[] = "{ \"foo\": ";

class SyncAuthTest : public SyncTest {
 public:
  SyncAuthTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SyncAuthTest() {}

  // Helper function that adds a bookmark with index |bookmark_index| and waits
  // for sync to complete. The return value indicates whether the sync cycle
  // completed successfully (true) or encountered an auth error (false).
  bool AddBookmarkAndWaitForSync(int bookmark_index) {
    std::wstring title = base::StringPrintf(L"Bookmark %d", bookmark_index);
    GURL url = GURL(base::StringPrintf("http://www.foo%d.com", bookmark_index));
    EXPECT_TRUE(AddURL(0, title, url) != NULL);
    return GetClient(0)->AwaitFullSyncCompletion("Added a bookmark.");
  }

  // Sets the authenticated state of the python sync server to |auth_state| and
  // sets the canned response that will be returned to the OAuth2TokenService
  // when it tries to fetch an access token.
  void SetAuthStateAndTokenResponse(PythonServerAuthState auth_state,
                                    const std::string& response_data,
                                    net::HttpStatusCode response_code,
                                    net::URLRequestStatus::Status status) {
    TriggerAuthState(auth_state);

    // If ProfileSyncService observes a transient error like SERVICE_UNAVAILABLE
    // or CONNECTION_FAILED, this means the OAuth2TokenService has given up
    // trying to reach Gaia. In practice, OA2TS retries a fixed number of times,
    // but the count is transparent to PSS.
    // Override the max retry count in TokenService so that we instantly trigger
    // the case where ProfileSyncService must pick up where OAuth2TokenService
    // left off (in terms of retries).
    ProfileOAuth2TokenServiceFactory::GetForProfile(GetProfile(0))->
        set_max_authorization_token_fetch_retries_for_testing(0);

    SetOAuth2TokenResponse(response_data, response_code, status);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncAuthTest);
};

// Verify that sync works with a valid OAuth2 token.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, Sanity) {
  ASSERT_TRUE(SetupSync());
  SetAuthStateAndTokenResponse(AUTHENTICATED_TRUE,
                               kValidOAuth2Token,
                               net::HTTP_OK,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_TRUE(AddBookmarkAndWaitForSync(1));
}

// Verify that ProfileSyncService continues trying to fetch access tokens
// when OAuth2TokenService has encountered more than a fixed number of
// HTTP_INTERNAL_SERVER_ERROR (500) errors.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryOnInternalServerError500) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddBookmarkAndWaitForSync(1));
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kValidOAuth2Token,
                               net::HTTP_INTERNAL_SERVER_ERROR,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_FALSE(AddBookmarkAndWaitForSync(2));
  ASSERT_TRUE(
      GetClient(0)->service()->IsRetryingAccessTokenFetchForTest());
}

// Verify that ProfileSyncService continues trying to fetch access tokens
// when OAuth2TokenService has encountered more than a fixed number of
// HTTP_FORBIDDEN (403) errors.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryOnHttpForbidden403) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddBookmarkAndWaitForSync(1));
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kEmptyOAuth2Token,
                               net::HTTP_FORBIDDEN,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_FALSE(AddBookmarkAndWaitForSync(2));
  ASSERT_TRUE(
      GetClient(0)->service()->IsRetryingAccessTokenFetchForTest());
}

// Verify that ProfileSyncService continues trying to fetch access tokens
// when OAuth2TokenService has encountered a URLRequestStatus of FAILED.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryOnRequestFailed) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddBookmarkAndWaitForSync(1));
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kEmptyOAuth2Token,
                               net::HTTP_INTERNAL_SERVER_ERROR,
                               net::URLRequestStatus::FAILED);
  ASSERT_FALSE(AddBookmarkAndWaitForSync(2));
  ASSERT_TRUE(
      GetClient(0)->service()->IsRetryingAccessTokenFetchForTest());
}

// Verify that ProfileSyncService continues trying to fetch access tokens
// when OAuth2TokenService receives a malformed token.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryOnMalformedToken) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddBookmarkAndWaitForSync(1));
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kMalformedOAuth2Token,
                               net::HTTP_OK,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_FALSE(AddBookmarkAndWaitForSync(2));
  ASSERT_TRUE(
      GetClient(0)->service()->IsRetryingAccessTokenFetchForTest());
}

// Verify that ProfileSyncService ends up with an INVALID_GAIA_CREDENTIALS auth
// error when an invalid_grant error is returned by OAuth2TokenService with an
// HTTP_BAD_REQUEST (400) response code.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, InvalidGrant) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddBookmarkAndWaitForSync(1));
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kInvalidGrantOAuth2Token,
                               net::HTTP_BAD_REQUEST,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_FALSE(AddBookmarkAndWaitForSync(2));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            GetClient(0)->service()->GetAuthError().state());
}

// Verify that ProfileSyncService ends up with an SERVICE_ERROR auth error when
// an invalid_client error is returned by OAuth2TokenService with an
// HTTP_BAD_REQUEST (400) response code.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, InvalidClient) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddBookmarkAndWaitForSync(1));
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kInvalidClientOAuth2Token,
                               net::HTTP_BAD_REQUEST,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_FALSE(AddBookmarkAndWaitForSync(2));
  ASSERT_EQ(GoogleServiceAuthError::SERVICE_ERROR,
            GetClient(0)->service()->GetAuthError().state());
}

// Verify that ProfileSyncService ends up with a REQUEST_CANCELED auth error
// when when OAuth2TokenService has encountered a URLRequestStatus of CANCELED.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RequestCanceled) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AddBookmarkAndWaitForSync(1));
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kEmptyOAuth2Token,
                               net::HTTP_INTERNAL_SERVER_ERROR,
                               net::URLRequestStatus::CANCELED);
  ASSERT_FALSE(AddBookmarkAndWaitForSync(2));
  ASSERT_EQ(GoogleServiceAuthError::REQUEST_CANCELED,
            GetClient(0)->service()->GetAuthError().state());
}

// Verify that ProfileSyncService fails initial sync setup during backend
// initialization and ends up with an INVALID_GAIA_CREDENTIALS auth error when
// an invalid_grant error is returned by OAuth2TokenService with an
// HTTP_BAD_REQUEST (400) response code.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, FailInitialSetupWithPersistentError) {
  ASSERT_TRUE(SetupClients());
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kInvalidGrantOAuth2Token,
                               net::HTTP_BAD_REQUEST,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_FALSE(GetClient(0)->SetupSync());
  ASSERT_FALSE(GetClient(0)->service()->sync_initialized());
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            GetClient(0)->service()->GetAuthError().state());
}

// Verify that ProfileSyncService fails initial sync setup during backend
// initialization, but continues trying to fetch access tokens when
// OAuth2TokenService receives an HTTP_INTERNAL_SERVER_ERROR (500) response
// code.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, RetryInitialSetupWithTransientError) {
  ASSERT_TRUE(SetupClients());
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kEmptyOAuth2Token,
                               net::HTTP_INTERNAL_SERVER_ERROR,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_FALSE(GetClient(0)->SetupSync());
  ASSERT_FALSE(GetClient(0)->service()->sync_initialized());
  ASSERT_TRUE(
      GetClient(0)->service()->IsRetryingAccessTokenFetchForTest());
}

// Verify that ProfileSyncService fetches a new token when an old token expires.
IN_PROC_BROWSER_TEST_F(SyncAuthTest, TokenExpiry) {
  // Initial sync succeeds with a short lived OAuth2 Token.
  ASSERT_TRUE(SetupClients());
  SetAuthStateAndTokenResponse(AUTHENTICATED_TRUE,
                               kShortLivedOAuth2Token,
                               net::HTTP_OK,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_TRUE(GetClient(0)->SetupSync());
  std::string old_token = GetClient(0)->service()->GetAccessTokenForTest();

  // Wait until the token has expired.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(5));

  // Trigger an auth error on the server so PSS requests OA2TS for a new token
  // during the next sync cycle.
  SetAuthStateAndTokenResponse(AUTHENTICATED_FALSE,
                               kEmptyOAuth2Token,
                               net::HTTP_INTERNAL_SERVER_ERROR,
                               net::URLRequestStatus::SUCCESS);
  ASSERT_FALSE(AddBookmarkAndWaitForSync(1));
  ASSERT_TRUE(
      GetClient(0)->service()->IsRetryingAccessTokenFetchForTest());

  // Trigger an auth success state and set up a new valid OAuth2 token.
  SetAuthStateAndTokenResponse(AUTHENTICATED_TRUE,
                               kValidOAuth2Token,
                               net::HTTP_OK,
                               net::URLRequestStatus::SUCCESS);

  // Verify that the next sync cycle is successful, and uses the new auth token.
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Syncing with new token."));
  std::string new_token = GetClient(0)->service()->GetAccessTokenForTest();
  ASSERT_NE(old_token, new_token);
}
