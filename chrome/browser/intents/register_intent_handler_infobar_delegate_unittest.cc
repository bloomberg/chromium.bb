// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/web_intent_service_data.h"

using content::BrowserThread;

namespace {

class MockWebIntentsRegistry : public WebIntentsRegistry {
 public:
  MOCK_METHOD1(RegisterIntentService,
               void(const webkit_glue::WebIntentServiceData&));
};

ProfileKeyedService* BuildMockWebIntentsRegistry(Profile* profile) {
  return new MockWebIntentsRegistry;
}

MockWebIntentsRegistry* BuildForProfile(Profile* profile) {
  return static_cast<MockWebIntentsRegistry*>(
      WebIntentsRegistryFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, BuildMockWebIntentsRegistry));
}

class RegisterIntentHandlerInfoBarDelegateTest
    : public TabContentsWrapperTestHarness {
 protected:
  RegisterIntentHandlerInfoBarDelegateTest()
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()) {}

  virtual void SetUp() {
    TabContentsWrapperTestHarness::SetUp();

    profile()->CreateWebDataService(false);
    web_intents_registry_ = BuildForProfile(profile());
  }

  virtual void TearDown() {
    web_intents_registry_ = NULL;

    TabContentsWrapperTestHarness::TearDown();
  }

  MockWebIntentsRegistry* web_intents_registry_;

 private:
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(RegisterIntentHandlerInfoBarDelegateTest);
};

TEST_F(RegisterIntentHandlerInfoBarDelegateTest, Accept) {
  webkit_glue::WebIntentServiceData service;
  service.service_url = GURL("google.com");
  service.action = ASCIIToUTF16("http://webintents.org/share");
  service.type = ASCIIToUTF16("text/url");
  RegisterIntentHandlerInfoBarDelegate delegate(
      contents_wrapper()->infobar_tab_helper(),
      WebIntentsRegistryFactory::GetForProfile(profile()),
      service, NULL, GURL());

  EXPECT_CALL(*web_intents_registry_, RegisterIntentService(service));
  delegate.Accept();
}

}  // namespace
