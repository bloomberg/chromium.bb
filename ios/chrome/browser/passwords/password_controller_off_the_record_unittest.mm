// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/passwords/password_controller.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class IncognitoPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  bool IsOffTheRecord() const override { return true; }
};

}  // namespace

class OffTheRecordWebState : public web::TestWebState {
 public:
  OffTheRecordWebState() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  web::BrowserState* GetBrowserState() const override {
    return chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  }

 private:
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

class PasswordControllerOffTheRecordTest : public web::WebTestWithWebState {
 public:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    password_controller_ = [[PasswordController alloc]
           initWithWebState:&off_the_record_web_state_
        passwordsUiDelegate:nil
                     client:base::MakeUnique<IncognitoPasswordManagerClient>()];
  }

  void TearDown() override {
    [password_controller_ detach];
    web::WebTestWithWebState::TearDown();
  }

 protected:
  OffTheRecordWebState off_the_record_web_state_;
  PasswordController* password_controller_;
};

// Check that if the PasswordController is told (by the PasswordManagerClient)
// that this is Incognito, it won't enable password generation.
TEST_F(PasswordControllerOffTheRecordTest, PasswordGenerationDisabled) {
  EXPECT_FALSE([this->password_controller_ passwordGenerationManager]);
}
