// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"
#include "chrome/browser/intents/web_intents_registry.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/intents/web_intent_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockWebIntentsRegistry : public WebIntentsRegistry {
 public:
  MOCK_METHOD1(RegisterIntentProvider, void(const WebIntentData&));
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
      : ui_thread_(BrowserThread::UI, MessageLoopForUI::current()) {}

  virtual void SetUp() {
    ChromeRenderViewHostTestHarness::SetUp();

    profile()->CreateWebDataService(false);

    SiteInstance* instance = SiteInstance::CreateSiteInstance(profile());
    tab_contents_.reset(new TestTabContents(profile(), instance));

    web_intents_registry_ = BuildForProfile(profile());
  }

  virtual void TearDown() {
    tab_contents_.reset();
    web_intents_registry_ = NULL;

    ChromeRenderViewHostTestHarness::TearDown();
  }

  scoped_ptr<TestTabContents> tab_contents_;
  MockWebIntentsRegistry* web_intents_registry_;

 private:
  BrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(RegisterIntentHandlerInfoBarDelegateTest);
};

TEST_F(RegisterIntentHandlerInfoBarDelegateTest, Accept) {
  WebIntentData intent;
  intent.service_url = GURL("google.com");
  intent.action = ASCIIToUTF16("http://webintents.org/share");
  intent.type = ASCIIToUTF16("text/url");
  RegisterIntentHandlerInfoBarDelegate delegate(tab_contents_.get(), intent);

  EXPECT_CALL(*web_intents_registry_, RegisterIntentProvider(intent));
  delegate.Accept();
}

}  // namespace
