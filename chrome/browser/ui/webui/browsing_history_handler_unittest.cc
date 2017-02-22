// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browsing_history_handler.h"

#include <stdint.h>
#include <memory>
#include <set>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/history/browsing_history_service.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/test_profile_sync_service.h"
#include "components/history/core/test/fake_web_history_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/sync/base/model_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

base::Time PretendNow() {
  base::Time::Exploded exploded_reference_time;
  exploded_reference_time.year = 2015;
  exploded_reference_time.month = 1;
  exploded_reference_time.day_of_month = 2;
  exploded_reference_time.day_of_week = 5;
  exploded_reference_time.hour = 11;
  exploded_reference_time.minute = 0;
  exploded_reference_time.second = 0;
  exploded_reference_time.millisecond = 0;

  base::Time out_time;
  EXPECT_TRUE(
      base::Time::FromLocalExploded(exploded_reference_time, &out_time));
  return out_time;
}

void IgnoreBoolAndDoNothing(bool ignored_argument) {}

class TestSyncService : public browser_sync::TestProfileSyncService {
 public:
  explicit TestSyncService(Profile* profile)
      : browser_sync::TestProfileSyncService(
            CreateProfileSyncServiceParamsForTest(profile)),
        sync_active_(true) {}

  bool IsSyncActive() const override { return sync_active_; }

  syncer::ModelTypeSet GetActiveDataTypes() const override {
    return syncer::ModelTypeSet::All();
  }

  void SetSyncActive(bool active) {
    sync_active_ = active;
    NotifyObservers();
  }

 private:
  bool sync_active_;
};

class BrowsingHistoryHandlerWithWebUIForTesting
    : public BrowsingHistoryHandler {
 public:
  explicit BrowsingHistoryHandlerWithWebUIForTesting(content::WebUI* web_ui)
      : test_clock_(new base::SimpleTestClock()) {
    set_clock(base::WrapUnique(test_clock_));
    set_web_ui(web_ui);
    test_clock_->SetNow(PretendNow());

  }

  base::SimpleTestClock* test_clock() { return test_clock_; }

 private:
  base::SimpleTestClock* test_clock_;
};

}  // namespace

class BrowsingHistoryHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              &BuildFakeProfileOAuth2TokenService);
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              &BuildFakeSigninManagerBase);
    builder.AddTestingFactory(ProfileSyncServiceFactory::GetInstance(),
                              &BuildFakeSyncService);
    builder.AddTestingFactory(WebHistoryServiceFactory::GetInstance(),
                              &BuildFakeWebHistoryService);
    profile_ = builder.Build();
    profile_->CreateBookmarkModel(false);

    sync_service_ = static_cast<TestSyncService*>(
        ProfileSyncServiceFactory::GetForProfile(profile_.get()));
    web_history_service_ = static_cast<history::FakeWebHistoryService*>(
        WebHistoryServiceFactory::GetForProfile(profile_.get()));

    web_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get())));
    web_ui_.reset(new content::TestWebUI);
    web_ui_->set_web_contents(web_contents_.get());
  }

  void TearDown() override {
    web_contents_.reset();
    web_ui_.reset();
    profile_.reset();
  }

  Profile* profile() { return profile_.get(); }
  TestSyncService* sync_service() { return sync_service_; }
  history::WebHistoryService* web_history_service() {
    return web_history_service_;
  }
  content::TestWebUI* web_ui() { return web_ui_.get(); }

 private:
  static std::unique_ptr<KeyedService> BuildFakeSyncService(
      content::BrowserContext* context) {
    return base::MakeUnique<TestSyncService>(
        static_cast<TestingProfile*>(context));
  }

  static std::unique_ptr<KeyedService> BuildFakeWebHistoryService(
      content::BrowserContext* context) {
    Profile* profile = static_cast<TestingProfile*>(context);

    std::unique_ptr<history::FakeWebHistoryService> service =
        base::MakeUnique<history::FakeWebHistoryService>(
            ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
            SigninManagerFactory::GetForProfile(profile),
            profile->GetRequestContext());
    service->SetupFakeResponse(true /* success */, net::HTTP_OK);
    return std::move(service);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  TestSyncService* sync_service_;
  history::FakeWebHistoryService* web_history_service_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(BrowsingHistoryHandlerTest, SetQueryTimeInWeeks) {
  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  history::QueryOptions options;

  // Querying this week should result in end time being midnight tonight and
  // begin time being midnight a week ago.
  handler.SetQueryTimeInWeeks(0, &options);
  base::Time midnight_tonight =
      PretendNow().LocalMidnight() + base::TimeDelta::FromDays(1);
  EXPECT_EQ(midnight_tonight, options.end_time);
  base::Time midnight_a_week_ago =
      midnight_tonight - base::TimeDelta::FromDays(7);
  EXPECT_EQ(midnight_a_week_ago, options.begin_time);

  // Querying 4 weeks ago should result in end time being midnight 4 weeks ago
  // and begin time being midnight 5 weeks ago.
  handler.SetQueryTimeInWeeks(4, &options);
  base::Time midnight_4_weeks_ago =
      PretendNow().LocalMidnight() - base::TimeDelta::FromDays(27);
  EXPECT_EQ(midnight_4_weeks_ago, options.end_time);
  base::Time midnight_5_weeks_ago =
      midnight_4_weeks_ago - base::TimeDelta::FromDays(7);
  EXPECT_EQ(midnight_5_weeks_ago, options.begin_time);
}

TEST_F(BrowsingHistoryHandlerTest, SetQueryTimeInMonths) {
  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  history::QueryOptions options;

  base::Time::Exploded exploded_expected_time;
  PretendNow().LocalExplode(&exploded_expected_time);

  // Querying this month should result in end time being unset and begin time
  // being midnight of the first.
  handler.SetQueryTimeInMonths(0, &options);
  EXPECT_EQ(base::Time(), options.end_time);

  exploded_expected_time.day_of_month = 1;
  exploded_expected_time.hour = 0;
  base::Time first_jan_2015_midnight;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded_expected_time,
                                            &first_jan_2015_midnight));
  EXPECT_EQ(first_jan_2015_midnight, options.begin_time);

  // Querying 6 months ago should result in end time being 2014-08-01 and begin
  // time being 2014-07-01.
  handler.SetQueryTimeInMonths(6, &options);

  exploded_expected_time.year = 2014;
  exploded_expected_time.month = 8;
  base::Time first_aug_2014_midnight;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded_expected_time,
                                            &first_aug_2014_midnight));
  EXPECT_EQ(first_aug_2014_midnight, options.end_time);
  exploded_expected_time.month = 7;
  base::Time first_jul_2014_midnight;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded_expected_time,
                                            &first_jul_2014_midnight));
  EXPECT_EQ(first_jul_2014_midnight, options.begin_time);
}

// Tests that BrowsingHistoryHandler is informed about WebHistoryService
// deletions.
TEST_F(BrowsingHistoryHandlerTest, ObservingWebHistoryDeletions) {
  base::Callback<void(bool)> callback = base::Bind(&IgnoreBoolAndDoNothing);

  // BrowsingHistoryHandler is informed about WebHistoryService history
  // deletions.
  {
    sync_service()->SetSyncActive(true);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();

    web_history_service()->ExpireHistoryBetween(std::set<GURL>(), base::Time(),
                                                base::Time::Max(), callback);

    EXPECT_EQ(1U, web_ui()->call_data().size());
    EXPECT_EQ("historyDeleted", web_ui()->call_data().back()->function_name());
  }

  // BrowsingHistoryHandler will be informed about WebHistoryService deletions
  // even if history sync is activated later.
  {
    sync_service()->SetSyncActive(false);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();
    sync_service()->SetSyncActive(true);

    web_history_service()->ExpireHistoryBetween(std::set<GURL>(), base::Time(),
                                                base::Time::Max(), callback);

    EXPECT_EQ(2U, web_ui()->call_data().size());
    EXPECT_EQ("historyDeleted", web_ui()->call_data().back()->function_name());
  }

  // BrowsingHistoryHandler does not fire historyDeleted while a web history
  // delete request is happening.
  {
    sync_service()->SetSyncActive(true);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();

    // Simulate an ongoing delete request.
    handler.browsing_history_service_->has_pending_delete_request_ = true;

    web_history_service()->ExpireHistoryBetween(
        std::set<GURL>(), base::Time(), base::Time::Max(),
        base::Bind(
            &BrowsingHistoryService::RemoveWebHistoryComplete,
            handler.browsing_history_service_->weak_factory_.GetWeakPtr()));

    EXPECT_EQ(3U, web_ui()->call_data().size());
    EXPECT_EQ("deleteComplete", web_ui()->call_data().back()->function_name());
  }

  // When history sync is not active, we don't listen to WebHistoryService
  // deletions. The WebHistoryService object still exists (because it's a
  // BrowserContextKeyedService), but is not visible to BrowsingHistoryHandler.
  {
    sync_service()->SetSyncActive(false);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();

    web_history_service()->ExpireHistoryBetween(std::set<GURL>(), base::Time(),
                                                base::Time::Max(), callback);

    // No additional WebUI calls were made.
    EXPECT_EQ(3U, web_ui()->call_data().size());
  }
}

#if !defined(OS_ANDROID)
TEST_F(BrowsingHistoryHandlerTest, MdTruncatesTitles) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kMaterialDesignHistory);

  history::URLResult long_result(
      GURL("http://looooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
      "ngurlislong.com"), base::Time());
  ASSERT_GT(long_result.url().spec().size(), 300u);

  history::QueryResults results;
  results.AppendURLBySwapping(&long_result);

  BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
  handler.RegisterMessages();  // Needed to create BrowsingHistoryService.
  web_ui()->ClearTrackedCalls();

  handler.browsing_history_service_->QueryComplete(
      base::string16(), history::QueryOptions(), &results);
  ASSERT_FALSE(web_ui()->call_data().empty());

  const base::ListValue* arg2;
  ASSERT_TRUE(web_ui()->call_data().front()->arg2()->GetAsList(&arg2));

  const base::DictionaryValue* first_entry;
  ASSERT_TRUE(arg2->GetDictionary(0, &first_entry));

  base::string16 title;
  ASSERT_TRUE(first_entry->GetString("title", &title));

  ASSERT_EQ(0u, title.find(base::ASCIIToUTF16("http://loooo")));
  EXPECT_EQ(300u, title.size());
}
#endif
