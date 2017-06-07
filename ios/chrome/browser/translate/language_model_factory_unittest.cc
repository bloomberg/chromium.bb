// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/language_model_factory.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::IsNull;
using testing::Not;

class LanguageModelFactoryTest : public testing::Test {
 public:
  LanguageModelFactoryTest() {
    TestChromeBrowserState::Builder browser_state_builder;
    chrome_browser_state_ = browser_state_builder.Build();
  }

  ~LanguageModelFactoryTest() override { chrome_browser_state_.reset(); }

  ios::ChromeBrowserState* chrome_browser_state() {
    return chrome_browser_state_.get();
  }

 private:
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(LanguageModelFactoryTest, NotCreatedInIncognito) {
  EXPECT_THAT(LanguageModelFactory::GetForBrowserState(chrome_browser_state()),
              Not(IsNull()));

  ios::ChromeBrowserState* otr_browser_state =
      chrome_browser_state()->GetOffTheRecordChromeBrowserState();
  translate::LanguageModel* language_model =
      LanguageModelFactory::GetForBrowserState(otr_browser_state);
  EXPECT_THAT(language_model, IsNull());
}
