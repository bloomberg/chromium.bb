// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_status.h"

using bookmarks_helper::AddURL;

const char kValidOAuth2Token[] = "{"
                                 "  \"refresh_token\": \"rt1\","
                                 "  \"access_token\": \"at1\","
                                 "  \"expires_in\": 3600,"  // 1 hour.
                                 "  \"token_type\": \"Bearer\""
                                 "}";

const char kInvalidOAuth2Token[] = "{"
                                   "  \"error\": \"invalid_grant\""
                                   "}";

class SyncAuthTest : public SyncTest {
 public:
  SyncAuthTest() : SyncTest(SINGLE_CLIENT) {}
  virtual ~SyncAuthTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncAuthTest);
};

IN_PROC_BROWSER_TEST_F(SyncAuthTest, Sanity) {
  ASSERT_TRUE(SetupSync());

  TriggerAuthState(AUTHENTICATED_TRUE);
  SetOAuth2TokenResponse(kValidOAuth2Token, net::HTTP_OK,
                         net::URLRequestStatus::SUCCESS);

  ASSERT_TRUE(AddURL(0, L"Bookmark 1", GURL("http://www.foo1.com")) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitFullSyncCompletion("Added a bookmark."));

  TriggerAuthState(AUTHENTICATED_FALSE);
  SetOAuth2TokenResponse(kInvalidOAuth2Token, net::HTTP_BAD_REQUEST,
                         net::URLRequestStatus::SUCCESS);

  ASSERT_TRUE(AddURL(0, L"Bookmark 2", GURL("http://www.foo2.com")) != NULL);
  ASSERT_FALSE(GetClient(0)->AwaitFullSyncCompletion("Added second bookmark."));
  ASSERT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS,
            GetClient(0)->service()->GetAuthError().state());
}
