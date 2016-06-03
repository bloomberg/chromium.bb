// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_controller.h"

#include <memory>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/web/public/test/test_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Return;

class MockWebState : public web::TestWebState {
 public:
  MOCK_CONST_METHOD0(GetBrowserState, web::BrowserState*(void));
};

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  // |form_manager| stays owned by the mock.
  MOCK_METHOD3(PromptUserToSaveOrUpdatePasswordPtr,
               void(password_manager::PasswordFormManager* form_manager,
                    password_manager::CredentialSourceType type,
                    bool update_password));
  MOCK_CONST_METHOD0(GetLogManager, password_manager::LogManager*(void));

  // Workaround for std::unique_ptr<> lacking a copy constructor.
  bool PromptUserToSaveOrUpdatePassword(
      std::unique_ptr<password_manager::PasswordFormManager> manager,
      password_manager::CredentialSourceType type,
      bool update_password) override {
    PromptUserToSaveOrUpdatePasswordPtr(manager.get(), type, update_password);
    return false;
  }
};

class MockLogManager : public password_manager::LogManager {
 public:
  MOCK_CONST_METHOD1(LogSavePasswordProgress, void(const std::string& text));
  MOCK_CONST_METHOD0(IsLoggingActive, bool(void));

  // Methods not important for testing.
  void OnLogRouterAvailabilityChanged(bool router_can_be_used) override {}
  void SetSuspended(bool suspended) override {}
};

TEST(PasswordControllerTest, SaveOnNonHTMLLandingPage) {
  // Create the PasswordController with a MockPasswordManagerClient.
  TestChromeBrowserState::Builder builder;
  auto pref_service =
      base::WrapUnique(new syncable_prefs::TestingPrefServiceSyncable);
  pref_service->registry()->RegisterBooleanPref(
      password_manager::prefs::kPasswordManagerSavingEnabled, true);
  builder.SetPrefService(std::move(pref_service));
  std::unique_ptr<TestChromeBrowserState> browser_state(builder.Build());
  MockWebState web_state;
  ON_CALL(web_state, GetBrowserState())
      .WillByDefault(testing::Return(browser_state.get()));
  auto client = base::WrapUnique(new MockPasswordManagerClient);
  MockPasswordManagerClient* weak_client = client.get();
  base::scoped_nsobject<PasswordController> passwordController(
      [[PasswordController alloc] initWithWebState:&web_state
                               passwordsUiDelegate:nil
                                            client:std::move(client)]);

  // Use a mock LogManager to detect that OnPasswordFormsRendered has been
  // called. TODO(crbug.com/598672): this is a hack, we should modularize the
  // code better to allow proper unit-testing.
  MockLogManager log_manager;
  EXPECT_CALL(log_manager, IsLoggingActive()).WillRepeatedly(Return(true));
  EXPECT_CALL(log_manager,
              LogSavePasswordProgress(
                  "Message: \"PasswordManager::OnPasswordFormsRendered\"\n"));
  EXPECT_CALL(log_manager,
              LogSavePasswordProgress(testing::Ne(
                  "Message: \"PasswordManager::OnPasswordFormsRendered\"\n")))
      .Times(testing::AnyNumber());
  EXPECT_CALL(*weak_client, GetLogManager())
      .WillRepeatedly(Return(&log_manager));

  web_state.SetContentIsHTML(false);
  web_state.SetCurrentURL(GURL("https://example.com"));
  [passwordController webStateDidLoadPage:&web_state];
}
