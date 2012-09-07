// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_to_mobile_service.h"

#include "chrome/browser/signin/token_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kDummyString[] = "dummy";

class DummyNotificationSource {};

class MockChromeToMobileService : public ChromeToMobileService {
 public:
  MockChromeToMobileService();
  ~MockChromeToMobileService();

  MOCK_METHOD0(RequestAccessToken, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockChromeToMobileService);
};

MockChromeToMobileService::MockChromeToMobileService()
    : ChromeToMobileService(NULL) {}

MockChromeToMobileService::~MockChromeToMobileService() {}

class ChromeToMobileServiceTest : public testing::Test {
 public:
  ChromeToMobileServiceTest();
  virtual ~ChromeToMobileServiceTest();

 protected:
  MockChromeToMobileService service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeToMobileServiceTest);
};

ChromeToMobileServiceTest::ChromeToMobileServiceTest() {}

ChromeToMobileServiceTest::~ChromeToMobileServiceTest() {}

// Ensure that irrelevant notifications do not invalidate the access token.
TEST_F(ChromeToMobileServiceTest, IgnoreIrrelevantNotifications) {
  EXPECT_CALL(service_, RequestAccessToken()).Times(0);

  service_.SetAccessTokenForTest(kDummyString);
  ASSERT_FALSE(service_.GetAccessTokenForTest().empty());

  // Send dummy service/token details (should not request token).
  DummyNotificationSource dummy_source;
  TokenService::TokenAvailableDetails dummy_details(kDummyString, kDummyString);
  service_.Observe(chrome::NOTIFICATION_TOKEN_AVAILABLE,
      content::Source<DummyNotificationSource>(&dummy_source),
      content::Details<TokenService::TokenAvailableDetails>(&dummy_details));
  EXPECT_FALSE(service_.GetAccessTokenForTest().empty());
}

// Ensure that proper notifications invalidate the access token.
TEST_F(ChromeToMobileServiceTest, AuthenticateOnTokenAvailable) {
  EXPECT_CALL(service_, RequestAccessToken()).Times(0);

  service_.SetAccessTokenForTest(kDummyString);
  ASSERT_FALSE(service_.GetAccessTokenForTest().empty());

  // Send a Gaia OAuth2 Login service dummy token (should request token).
  DummyNotificationSource dummy_source;
  TokenService::TokenAvailableDetails login_details(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, kDummyString);
  service_.Observe(chrome::NOTIFICATION_TOKEN_AVAILABLE,
      content::Source<DummyNotificationSource>(&dummy_source),
      content::Details<TokenService::TokenAvailableDetails>(&login_details));
  EXPECT_TRUE(service_.GetAccessTokenForTest().empty());
}

}  // namespace
