// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/chrome_translate_client.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/user_events/fake_user_event_service.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

std::unique_ptr<KeyedService> BuildFakeUserEventService(
    content::BrowserContext* context) {
  return base::MakeUnique<syncer::FakeUserEventService>();
}

class ChromeTranslateClientTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    fake_user_event_service_ = static_cast<syncer::FakeUserEventService*>(
        browser_sync::UserEventServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(browser_context(),
                                      &BuildFakeUserEventService));
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(
        switches::kSyncUserLanguageDetectionEvents);
  }

  void TearDown() override { ChromeRenderViewHostTestHarness::TearDown(); }

 protected:
  syncer::FakeUserEventService* GetUserEventService() {
    return fake_user_event_service_;
  }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
  syncer::FakeUserEventService* fake_user_event_service_;
};

TEST_F(ChromeTranslateClientTest, LanguageEventShouldRecord) {
  GURL url("http://yahoo.com");
  NavigateAndCommit(url);
  ChromeTranslateClient client(web_contents());
  translate::LanguageDetectionDetails details;
  details.cld_language = "en";
  details.is_cld_reliable = true;
  details.adopted_language = "en";
  client.OnLanguageDetermined(details);
  EXPECT_EQ(1ul, GetUserEventService()->GetRecordedUserEvents().size());
}

TEST_F(ChromeTranslateClientTest, LanguageEventShouldNotRecord) {
  GURL url("about://blank");
  NavigateAndCommit(url);
  ChromeTranslateClient client(web_contents());
  translate::LanguageDetectionDetails details;
  details.cld_language = "en";
  details.is_cld_reliable = true;
  details.adopted_language = "en";
  client.OnLanguageDetermined(details);
  EXPECT_EQ(0u, GetUserEventService()->GetRecordedUserEvents().size());
}
