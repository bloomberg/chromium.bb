// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browsing_history_handler.h"

#include <stdint.h>
#include <memory>
#include <set>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
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

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/md_history_ui.h"
#endif

namespace {

struct TestResult {
  std::string url;
  int64_t hour_offset;  // Visit time in hours past the baseline time.
};

// Duplicates on the same day in the local timezone are removed, so set a
// baseline time in local time.
const base::Time baseline_time = base::Time::UnixEpoch().LocalMidnight();

// For each item in |results|, create a new Value representing the visit, and
// insert it into |list_value|.
void AddQueryResults(
    TestResult* test_results,
    int test_results_size,
    std::vector<BrowsingHistoryHandler::HistoryEntry>* results) {
  for (int i = 0; i < test_results_size; ++i) {
    BrowsingHistoryHandler::HistoryEntry entry;
    entry.time = baseline_time +
                 base::TimeDelta::FromHours(test_results[i].hour_offset);
    entry.url = GURL(test_results[i].url);
    entry.all_timestamps.insert(entry.time.ToInternalValue());
    results->push_back(entry);
  }
}

// Returns true if |result| matches the test data given by |correct_result|,
// otherwise returns false.
bool ResultEquals(
    const BrowsingHistoryHandler::HistoryEntry& result,
    const TestResult& correct_result) {
  base::Time correct_time =
      baseline_time + base::TimeDelta::FromHours(correct_result.hour_offset);

  return result.time == correct_time && result.url == GURL(correct_result.url);
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
  explicit BrowsingHistoryHandlerWithWebUIForTesting(content::WebUI* web_ui) {
    set_web_ui(web_ui);
  }
  using BrowsingHistoryHandler::QueryComplete;
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

// Tests that the MergeDuplicateResults method correctly removes duplicate
// visits to the same URL on the same day.
// Fails on Android.  http://crbug.com/2345
#if defined(OS_ANDROID)
#define MAYBE_MergeDuplicateResults DISABLED_MergeDuplicateResults
#else
#define MAYBE_MergeDuplicateResults MergeDuplicateResults
#endif
TEST_F(BrowsingHistoryHandlerTest, MAYBE_MergeDuplicateResults) {
  {
    // Basic test that duplicates on the same day are removed.
    TestResult test_data[] = {
      { "http://google.com", 0 },
      { "http://google.de", 1 },
      { "http://google.com", 2 },
      { "http://google.com", 3 }  // Most recent.
    };
    std::vector<BrowsingHistoryHandler::HistoryEntry> results;
    AddQueryResults(test_data, arraysize(test_data), &results);
    BrowsingHistoryHandler::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    EXPECT_TRUE(ResultEquals(results[0], test_data[3]));
    EXPECT_TRUE(ResultEquals(results[1], test_data[1]));
  }

  {
    // Test that a duplicate URL on the next day is not removed.
    TestResult test_data[] = {
      { "http://google.com", 0 },
      { "http://google.com", 23 },
      { "http://google.com", 24 },  // Most recent.
    };
    std::vector<BrowsingHistoryHandler::HistoryEntry> results;
    AddQueryResults(test_data, arraysize(test_data), &results);
    BrowsingHistoryHandler::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    EXPECT_TRUE(ResultEquals(results[0], test_data[2]));
    EXPECT_TRUE(ResultEquals(results[1], test_data[1]));
  }

  {
    // Test multiple duplicates across multiple days.
    TestResult test_data[] = {
      // First day.
      { "http://google.de", 0 },
      { "http://google.com", 1 },
      { "http://google.de", 2 },
      { "http://google.com", 3 },

      // Second day.
      { "http://google.de", 24 },
      { "http://google.com", 25 },
      { "http://google.de", 26 },
      { "http://google.com", 27 },  // Most recent.
    };
    std::vector<BrowsingHistoryHandler::HistoryEntry> results;
    AddQueryResults(test_data, arraysize(test_data), &results);
    BrowsingHistoryHandler::MergeDuplicateResults(&results);

    ASSERT_EQ(4U, results.size());
    EXPECT_TRUE(ResultEquals(results[0], test_data[7]));
    EXPECT_TRUE(ResultEquals(results[1], test_data[6]));
    EXPECT_TRUE(ResultEquals(results[2], test_data[3]));
    EXPECT_TRUE(ResultEquals(results[3], test_data[2]));
  }

  {
    // Test that timestamps for duplicates are properly saved.
    TestResult test_data[] = {
      { "http://google.com", 0 },
      { "http://google.de", 1 },
      { "http://google.com", 2 },
      { "http://google.com", 3 }  // Most recent.
    };
    std::vector<BrowsingHistoryHandler::HistoryEntry> results;
    AddQueryResults(test_data, arraysize(test_data), &results);
    BrowsingHistoryHandler::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    EXPECT_TRUE(ResultEquals(results[0], test_data[3]));
    EXPECT_TRUE(ResultEquals(results[1], test_data[1]));
    EXPECT_EQ(3u, results[0].all_timestamps.size());
    EXPECT_EQ(1u, results[1].all_timestamps.size());
  }
}

// Tests that BrowsingHistoryHandler observes WebHistoryService deletions.
TEST_F(BrowsingHistoryHandlerTest, ObservingWebHistoryDeletions) {
  base::Callback<void(bool)> callback = base::Bind(&IgnoreBoolAndDoNothing);

  // BrowsingHistoryHandler listens to WebHistoryService history deletions.
  {
    sync_service()->SetSyncActive(true);
    BrowsingHistoryHandlerWithWebUIForTesting handler(web_ui());
    handler.RegisterMessages();

    web_history_service()->ExpireHistoryBetween(std::set<GURL>(), base::Time(),
                                                base::Time::Max(), callback);

    EXPECT_EQ(1U, web_ui()->call_data().size());
    EXPECT_EQ("historyDeleted", web_ui()->call_data().back()->function_name());
  }

  // BrowsingHistoryHandler will listen to WebHistoryService deletions even if
  // history sync is activated later.
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
    handler.has_pending_delete_request_ = true;

    web_history_service()->ExpireHistoryBetween(
        std::set<GURL>(), base::Time(), base::Time::Max(),
        base::Bind(&BrowsingHistoryHandler::RemoveWebHistoryComplete,
                   handler.weak_factory_.GetWeakPtr()));

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
  MdHistoryUI::SetEnabledForTesting(true);

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
  web_ui()->ClearTrackedCalls();

  handler.QueryComplete(base::string16(), history::QueryOptions(), &results);
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
