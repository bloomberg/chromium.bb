// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
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
    : public ChromeRenderViewHostTestHarness {
 protected:
  RegisterIntentHandlerInfoBarDelegateTest()
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()),
        db_thread_(BrowserThread::DB, MessageLoopForUI::current()) {}

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();

    profile()->CreateWebDataService();
    web_intents_registry_ = BuildForProfile(profile());
  }

  virtual void TearDown() {
    web_intents_registry_ = NULL;

    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockWebIntentsRegistry* web_intents_registry_;

 private:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;

  DISALLOW_COPY_AND_ASSIGN(RegisterIntentHandlerInfoBarDelegateTest);
};

TEST_F(RegisterIntentHandlerInfoBarDelegateTest, Accept) {
  webkit_glue::WebIntentServiceData service;
  service.service_url = GURL("google.com");
  service.action = ASCIIToUTF16("http://webintents.org/share");
  service.type = ASCIIToUTF16("text/url");
  scoped_ptr<RegisterIntentHandlerInfoBarDelegate> infobar_delegate(
      RegisterIntentHandlerInfoBarDelegate::Create(web_intents_registry_,
                                                   service));

  EXPECT_CALL(*web_intents_registry_, RegisterIntentService(service));
  infobar_delegate->Accept();
}

}  // namespace
