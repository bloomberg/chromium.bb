// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

#include "base/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/cryptohome/mock_cryptohome_library.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace chromeos {

class DeviceOAuth2TokenServiceTest : public testing::Test {
 public:
  DeviceOAuth2TokenServiceTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()) {}
  virtual ~DeviceOAuth2TokenServiceTest() {}

  virtual void SetUp() OVERRIDE {
  }

  virtual void TearDown() OVERRIDE {
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  ScopedTestingLocalState scoped_testing_local_state_;
};

TEST_F(DeviceOAuth2TokenServiceTest, SaveEncryptedToken) {
  StrictMock<MockCryptohomeLibrary> mock_cryptohome_library;
  CryptohomeLibrary::SetForTest(&mock_cryptohome_library);

  EXPECT_CALL(mock_cryptohome_library, DecryptWithSystemSalt(StrEq("")))
      .Times(1)
      .WillOnce(Return(""));
  EXPECT_CALL(mock_cryptohome_library,
              EncryptWithSystemSalt(StrEq("test-token")))
      .Times(1)
      .WillOnce(Return("encrypted"));
  EXPECT_CALL(mock_cryptohome_library,
              DecryptWithSystemSalt(StrEq("encrypted")))
      .Times(1)
      .WillOnce(Return("test-token"));

  DeviceOAuth2TokenService oauth2_service(
      new net::TestURLRequestContextGetter(message_loop_.message_loop_proxy()),
      scoped_testing_local_state_.Get());

  ASSERT_EQ("", oauth2_service.GetRefreshToken());
  oauth2_service.SetAndSaveRefreshToken("test-token");
  ASSERT_EQ("test-token", oauth2_service.GetRefreshToken());

  // This call won't invoke decrypt again, since the value is cached.
  ASSERT_EQ("test-token", oauth2_service.GetRefreshToken());
}

}  // namespace chromeos
