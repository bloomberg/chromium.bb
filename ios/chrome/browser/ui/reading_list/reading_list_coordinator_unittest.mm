// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/reading_list/core/reading_list_entry.h"
#include "components/reading_list/core/reading_list_model_impl.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_controller.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using favicon::PostReply;
using testing::_;

#pragma mark - UrlLoader

@interface UrlLoaderStub : NSObject<UrlLoader> {
  GURL _url;
  web::Referrer _referrer;
}

@property(nonatomic, readonly) const GURL& url;
@property(nonatomic, readonly) const web::Referrer& referrer;
@property(nonatomic, assign) ui::PageTransition transition;
@property(nonatomic, assign) BOOL rendererInitiated;
@property(nonatomic, assign) BOOL inIncognito;
@end

@implementation UrlLoaderStub

@synthesize transition = _transition;
@synthesize rendererInitiated = _rendererInitiated;
@synthesize inIncognito = _inIncognito;

- (void)loadURL:(const GURL&)url
             referrer:(const web::Referrer&)referrer
           transition:(ui::PageTransition)transition
    rendererInitiated:(BOOL)rendererInitiated {
  _url = url;
  _referrer = referrer;
  self.transition = transition;
  self.rendererInitiated = rendererInitiated;
}

- (void)webPageOrderedOpen:(const GURL&)url
                  referrer:(const web::Referrer&)referrer
              inBackground:(BOOL)inBackground
                  appendTo:(OpenPosition)appendTo {
}

- (void)webPageOrderedOpen:(const GURL&)url
                  referrer:(const web::Referrer&)referrer
               inIncognito:(BOOL)inIncognito
              inBackground:(BOOL)inBackground
                  appendTo:(OpenPosition)appendTo {
  _url = url;
  _referrer = referrer;
  self.inIncognito = inIncognito;
}

- (void)loadSessionTab:(const sessions::SessionTab*)sessionTab {
}

- (void)loadJavaScriptFromLocationBar:(NSString*)script {
}

- (const GURL&)url {
  return _url;
}

- (const web::Referrer&)referrer {
  return _referrer;
}

@end

#pragma mark - feature_engagement::Tracker
namespace feature_engagement {
namespace {

class TrackerStub : public feature_engagement::Tracker {
 public:
  MOCK_METHOD1(NotifyEvent, void(const std::string&));
  MOCK_METHOD1(ShouldTriggerHelpUI, bool(const base::Feature& feature));
  MOCK_METHOD1(GetTriggerState,
               Tracker::TriggerState(const base::Feature& feature));
  MOCK_METHOD1(Dismissed, void(const base::Feature& feature));
  MOCK_METHOD0(IsInitialized, bool());
  MOCK_METHOD1(AddOnInitializedCallback, void(OnInitializedCallback callback));
};

}  //  namespace
}  //  namespace feature_engagement

#pragma mark - ReadingListCoordinatorTest

class ReadingListCoordinatorTest : public web::WebTestWithWebState {
 public:
  ReadingListCoordinatorTest() {
    loader_mock_ = [[UrlLoaderStub alloc] init];

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        feature_engagement::TrackerFactory::GetInstance(),
        ReadingListCoordinatorTest::BuildFeatureEngagementTrackerStub);
    browser_state_ = builder.Build();

    reading_list_model_.reset(new ReadingListModelImpl(
        nullptr, nullptr, base::MakeUnique<base::DefaultClock>()));
    large_icon_service_.reset(new favicon::LargeIconService(
        &mock_favicon_service_, base::ThreadTaskRunnerHandle::Get(),
        /*image_fetcher=*/nullptr));
    mediator_ =
        [[ReadingListMediator alloc] initWithModel:reading_list_model_.get()
                                  largeIconService:large_icon_service_.get()];
    coordinator_ = [[ReadingListCoordinator alloc]
        initWithBaseViewController:nil
                      browserState:browser_state_.get()
                            loader:loader_mock_];
    coordinator_.mediator = mediator_;

    EXPECT_CALL(mock_favicon_service_,
                GetLargestRawFaviconForPageURL(_, _, _, _, _))
        .WillRepeatedly(PostReply<5>(favicon_base::FaviconRawBitmapResult()));
  }

  ~ReadingListCoordinatorTest() override {}

  ReadingListCoordinator* GetCoordinator() { return coordinator_; }

  ReadingListModel* GetReadingListModel() { return reading_list_model_.get(); }
  UrlLoaderStub* GetLoaderStub() { return loader_mock_; }

  ios::ChromeBrowserState* GetBrowserState() { return browser_state_.get(); }

  ReadingListCollectionViewController*
  GetAReadingListCollectionViewController() {
    return [[ReadingListCollectionViewController alloc]
        initWithDataSource:mediator_
                   toolbar:nil];
  }

  static std::unique_ptr<KeyedService> BuildFeatureEngagementTrackerStub(
      web::BrowserState*) {
    return base::MakeUnique<feature_engagement::TrackerStub>();
  }

 private:
  ReadingListCoordinator* coordinator_;
  ReadingListMediator* mediator_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  UrlLoaderStub* loader_mock_;
  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<favicon::LargeIconService> large_icon_service_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
};

// Tests that the implementation of ReadingListCoordinator openItemAtIndexPath
// opens the entry.
TEST_F(ReadingListCoordinatorTest, OpenItem) {
  // Setup.
  GURL url("https://chromium.org");
  std::string title("Chromium");
  ReadingListModel* model = GetReadingListModel();
  model->AddEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP);

  ReadingListCollectionViewItem* item = [[ReadingListCollectionViewItem alloc]
           initWithType:0
                    url:url
      distillationState:ReadingListUIDistillationStatusSuccess];

  // Action.
  [GetCoordinator() readingListCollectionViewController:
                        GetAReadingListCollectionViewController()
                                               openItem:item];

  // Tests.
  UrlLoaderStub* loader = GetLoaderStub();
  EXPECT_EQ(url, loader.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           loader.transition));
  EXPECT_EQ(NO, loader.rendererInitiated);
}

TEST_F(ReadingListCoordinatorTest, OpenItemOffline) {
  // Setup.
  GURL url("https://chromium.org");
  std::string title("Chromium");
  ReadingListModel* model = GetReadingListModel();
  model->AddEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP);
  base::FilePath distilled_path("test");
  GURL distilled_url("https://distilled.com");
  model->SetEntryDistilledInfo(url, distilled_path, distilled_url, 123,
                               base::Time::FromTimeT(10));

  ReadingListCollectionViewItem* item = [[ReadingListCollectionViewItem alloc]
           initWithType:0
                    url:url
      distillationState:ReadingListUIDistillationStatusSuccess];
  GURL offlineURL =
      reading_list::OfflineURLForPath(distilled_path, url, distilled_url);
  ASSERT_FALSE(model->GetEntryByURL(url)->IsRead());

  // Action.
  [GetCoordinator() readingListCollectionViewController:
                        GetAReadingListCollectionViewController()
                                openItemOfflineInNewTab:item];

  // Tests.
  UrlLoaderStub* loader = GetLoaderStub();
  EXPECT_EQ(offlineURL, loader.url);
  EXPECT_FALSE(loader.inIncognito);
  EXPECT_TRUE(model->GetEntryByURL(url)->IsRead());
}

TEST_F(ReadingListCoordinatorTest, OpenItemInNewTab) {
  // Setup.
  GURL url("https://chromium.org");
  std::string title("Chromium");
  ReadingListModel* model = GetReadingListModel();
  model->AddEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP);

  ReadingListCollectionViewItem* item = [[ReadingListCollectionViewItem alloc]
           initWithType:0
                    url:url
      distillationState:ReadingListUIDistillationStatusSuccess];

  // Action.
  [GetCoordinator() readingListCollectionViewController:
                        GetAReadingListCollectionViewController()
                                       openItemInNewTab:item
                                              incognito:YES];

  // Tests.
  UrlLoaderStub* loader = GetLoaderStub();
  EXPECT_EQ(url, loader.url);
  EXPECT_TRUE(loader.inIncognito);
}

TEST_F(ReadingListCoordinatorTest, SendViewedReadingListEventInStart) {
  // Setup.
  feature_engagement::TrackerStub* tracker =
      static_cast<feature_engagement::TrackerStub*>(
          feature_engagement::TrackerFactory::GetForBrowserState(
              GetBrowserState()));

  // Actions and Tests.
  EXPECT_CALL((*tracker),
              NotifyEvent(feature_engagement::events::kViewedReadingList));
  [GetCoordinator() start];
}
