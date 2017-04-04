// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_collection_view_controller.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#import "base/test/ios/wait_util.h"
#include "base/time/time.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_test_util.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/history/history_entry.h"
#import "ios/chrome/browser/ui/history/history_service_facade.h"
#import "ios/chrome/browser/ui/history/history_service_facade_delegate.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

HistoryServiceFacade::QueryResult QueryResultWithVisits(
    std::vector<std::pair<const GURL&, base::Time>> visits) {
  std::vector<history::HistoryEntry> entries{};
  for (std::pair<const GURL&, base::Time> visit : visits) {
    history::HistoryEntry entry = history::HistoryEntry();
    entry.url = visit.first;
    entry.time = visit.second;
    entries.push_back(entry);
  }
  HistoryServiceFacade::QueryResult result{};
  result.entries = entries;
  result.finished = true;
  return result;
}

std::unique_ptr<KeyedService> BuildMockSyncSetupService(
    web::BrowserState* context) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  syncer::SyncService* sync_service =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state);
  return base::MakeUnique<SyncSetupServiceMock>(sync_service,
                                                browser_state->GetPrefs());
}

}  // namespace

@interface HistoryCollectionViewController (
    Testing)<HistoryServiceFacadeDelegate>
- (void)didPressClearBrowsingBar;
@end

class HistoryCollectionViewControllerTest : public BlockCleanupTest {
 public:
  HistoryCollectionViewControllerTest() {}
  ~HistoryCollectionViewControllerTest() override {}

  void SetUp() override {
    BlockCleanupTest::SetUp();
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFake::CreateAuthenticationService);
    builder.AddTestingFactory(SyncSetupServiceFactory::GetInstance(),
                              &BuildMockSyncSetupService);
    mock_browser_state_ = builder.Build();
    sync_setup_service_mock_ = static_cast<SyncSetupServiceMock*>(
        SyncSetupServiceFactory::GetForBrowserState(mock_browser_state_.get()));
    mock_delegate_ = [OCMockObject
        niceMockForProtocol:@protocol(HistoryCollectionViewControllerDelegate)];
    mock_url_loader_ = [OCMockObject niceMockForProtocol:@protocol(UrlLoader)];
    history_collection_view_controller_ =
        [[HistoryCollectionViewController alloc]
            initWithLoader:mock_url_loader_
              browserState:mock_browser_state_.get()
                  delegate:mock_delegate_];
  }

  void TearDown() override {
    history_collection_view_controller_ = nil;
    BlockCleanupTest::TearDown();
  }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  id<UrlLoader> mock_url_loader_;
  std::unique_ptr<TestChromeBrowserState> mock_browser_state_;
  id<HistoryCollectionViewControllerDelegate> mock_delegate_;
  HistoryCollectionViewController* history_collection_view_controller_;
  bool privacy_settings_opened_;
  SyncSetupServiceMock* sync_setup_service_mock_;
  DISALLOW_COPY_AND_ASSIGN(HistoryCollectionViewControllerTest);
};

// Tests that hasHistoryEntries property returns YES after entries have been
// received.
TEST_F(HistoryCollectionViewControllerTest, HasHistoryEntries) {
  GURL url_1("http://test1");
  HistoryServiceFacade::QueryResult query_result =
      QueryResultWithVisits({{url_1, base::Time::Now()}});
  [history_collection_view_controller_ historyServiceFacade:nil
                                      didReceiveQueryResult:query_result];
  EXPECT_TRUE([history_collection_view_controller_ hasHistoryEntries]);
}

// Tests that local history items are shown when sync is enabled,
// HISTORY_DELETE_DIRECTIVES is enabled, and sync_returned is false.
// This ensures that when HISTORY_DELETE_DIRECTIVES is disabled,
// only local device history items are shown.
TEST_F(HistoryCollectionViewControllerTest, HasHistoryEntriesWhenSyncEnabled) {
  GURL url_1("http://test1");
  EXPECT_CALL(*sync_setup_service_mock_, IsSyncEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*sync_setup_service_mock_,
              IsDataTypeEnabled(syncer::HISTORY_DELETE_DIRECTIVES))
      .WillRepeatedly(testing::Return(false));

  HistoryServiceFacade::QueryResult query_result =
      QueryResultWithVisits({{url_1, base::Time::Now()}});
  [history_collection_view_controller_ historyServiceFacade:nil
                                      didReceiveQueryResult:query_result];
  EXPECT_TRUE([history_collection_view_controller_ hasHistoryEntries]);
}

// Tests adding two entries to history from the same day, then deleting the
// first of them results in one history entry in the collection.
TEST_F(HistoryCollectionViewControllerTest, DeleteSingleEntry) {
  // Add history entries
  GURL url_1("http://test1");
  GURL url_2("http://test2");
  HistoryServiceFacade::QueryResult query_result = QueryResultWithVisits(
      {{url_1, base::Time::Now()}, {url_2, base::Time::Now()}});
  [history_collection_view_controller_ historyServiceFacade:nil
                                      didReceiveQueryResult:query_result];

  UICollectionView* collection_view =
      [history_collection_view_controller_ collectionView];
  [collection_view
      selectItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
  [history_collection_view_controller_ deleteSelectedItemsFromHistory];

  // Expect header section with one item and one entries section with one item.
  EXPECT_EQ(2, [collection_view numberOfSections]);
  EXPECT_EQ(1, [collection_view numberOfItemsInSection:0]);
  EXPECT_EQ(1, [collection_view numberOfItemsInSection:1]);
}

// Tests that adding two entries to history from the same day then deleting
// both of them results in only the header section in the collection.
TEST_F(HistoryCollectionViewControllerTest, DeleteMultipleEntries) {
  // Add history entries.
  GURL url_1("http://test1");
  GURL url_2("http://test2");
  HistoryServiceFacade::QueryResult query_result = QueryResultWithVisits(
      {{url_1, base::Time::Now()}, {url_2, base::Time::Now()}});
  [history_collection_view_controller_ historyServiceFacade:nil
                                      didReceiveQueryResult:query_result];

  // Select history entries and tap delete.
  UICollectionView* collection_view =
      [history_collection_view_controller_ collectionView];
  collection_view.allowsMultipleSelection = YES;
  [collection_view
      selectItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
  [collection_view
      selectItemAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:1]
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
  EXPECT_EQ(2, (int)[[collection_view indexPathsForSelectedItems] count]);
  [history_collection_view_controller_ deleteSelectedItemsFromHistory];

  // Expect only the header section to remain.
  EXPECT_EQ(1, [collection_view numberOfSections]);
  EXPECT_EQ(1, [collection_view numberOfItemsInSection:0]);
}

// Tests that adding two entries to history from different days then deleting
// both of them results in only the header section in the collection.
TEST_F(HistoryCollectionViewControllerTest, DeleteMultipleSections) {
  GURL url_1("http://test1");
  GURL url_2("http://test2");

  HistoryServiceFacade::QueryResult query_result = QueryResultWithVisits(
      {{url_1, base::Time::Now() - base::TimeDelta::FromDays(1)},
       {url_2, base::Time::Now()}});
  [history_collection_view_controller_ historyServiceFacade:nil
                                      didReceiveQueryResult:query_result];
  UICollectionView* collection_view =
      [history_collection_view_controller_ collectionView];
  // Expect two history sections in addition to the header section.
  EXPECT_EQ(3, [collection_view numberOfSections]);

  // Select all history items, and delete.
  collection_view.allowsMultipleSelection = YES;
  [collection_view
      selectItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
  [collection_view
      selectItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:2]
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
  [history_collection_view_controller_ deleteSelectedItemsFromHistory];

  // Expect only the header section to remain.
  EXPECT_EQ(1, [collection_view numberOfSections]);
  EXPECT_EQ(1, [collection_view numberOfItemsInSection:0]);
}
