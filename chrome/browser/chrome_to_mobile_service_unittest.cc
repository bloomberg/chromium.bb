// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_to_mobile_service.h"

#include "chrome/browser/signin/token_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

const char kDummyString[] = "dummy";

class DummyNotificationSource {};

// A mock ChromeToMobileService with a mocked out RefreshAccessToken method.
class MockChromeToMobileService : public ChromeToMobileService {
 public:
  MockChromeToMobileService();
  ~MockChromeToMobileService();

  MOCK_METHOD0(RefreshAccessToken, void());

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

// Ensure that RefreshAccessToken is not called for irrelevant notifications.
TEST_F(ChromeToMobileServiceTest, IgnoreIrrelevantNotifications) {
  EXPECT_CALL(service_, RefreshAccessToken()).Times(0);

  // Send dummy service/token details (should not refresh token).
  DummyNotificationSource dummy_source;
  TokenService::TokenAvailableDetails dummy_details(kDummyString, kDummyString);
  service_.Observe(chrome::NOTIFICATION_TOKEN_AVAILABLE,
      content::Source<DummyNotificationSource>(&dummy_source),
      content::Details<TokenService::TokenAvailableDetails>(&dummy_details));
}

// Ensure that RefreshAccessToken is called on the proper notification.
TEST_F(ChromeToMobileServiceTest, AuthenticateOnTokenAvailable) {
  EXPECT_CALL(service_, RefreshAccessToken()).Times(1);

  // Send a Gaia OAuth2 Login service dummy token (should refresh token).
  DummyNotificationSource dummy_source;
  TokenService::TokenAvailableDetails login_details(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, kDummyString);
  service_.Observe(chrome::NOTIFICATION_TOKEN_AVAILABLE,
      content::Source<DummyNotificationSource>(&dummy_source),
      content::Details<TokenService::TokenAvailableDetails>(&login_details));
}

}  // namespace
