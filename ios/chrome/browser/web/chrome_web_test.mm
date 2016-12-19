// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/chrome_web_test.h"

#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/fullscreen_controller.h"
#import "ios/web/public/web_state/web_state.h"

ChromeWebTest::~ChromeWebTest() {}

ChromeWebTest::ChromeWebTest() : web::WebTestWithWebState() {
  TestChromeBrowserState::Builder browser_state_builder;
  chrome_browser_state_ = browser_state_builder.Build();
}

void ChromeWebTest::SetUp() {
  web::WebTestWithWebState::SetUp();
  IOSChromePasswordStoreFactory::GetInstance()->SetTestingFactory(
      chrome_browser_state_.get(),
      &password_manager::BuildPasswordStore<
          web::BrowserState, password_manager::MockPasswordStore>);
  [FullScreenController setEnabledForTests:NO];
}

void ChromeWebTest::TearDown() {
  WaitForBackgroundTasks();
  [FullScreenController setEnabledForTests:YES];
  web::WebTestWithWebState::TearDown();
}

web::BrowserState* ChromeWebTest::GetBrowserState() {
  return chrome_browser_state_.get();
}
