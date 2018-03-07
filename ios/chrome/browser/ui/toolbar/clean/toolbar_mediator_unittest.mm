// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_mediator.h"

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_consumer.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_web_state.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/test_branded_image_provider.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;
using bookmarks::BookmarkModel;

namespace {

// Test VoiceSearchProvider that allow overriding whether voice search
// is enabled or not.
class TestToolbarMediatorVoiceSearchProvider : public VoiceSearchProvider {
 public:
  TestToolbarMediatorVoiceSearchProvider() = default;
  ~TestToolbarMediatorVoiceSearchProvider() override = default;

  // Setter to control the value returned by IsVoiceSearchEnabled().
  void set_voice_search_enabled(bool enabled) {
    voice_search_enabled_ = enabled;
  }

  // VoiceSearchProvider implementation.
  bool IsVoiceSearchEnabled() const override { return voice_search_enabled_; }

 private:
  bool voice_search_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarMediatorVoiceSearchProvider);
};

// Test BrandedImageProvider that return different branded images for Google
// Search search engine.
class TestToolbarMediatorBrandedImageProvider
    : public TestBrandedImageProvider {
 public:
  TestToolbarMediatorBrandedImageProvider()
      : google_search_icon_([[UIImage alloc] init]),
        generic_search_icon_([[UIImage alloc] init]) {}

  ~TestToolbarMediatorBrandedImageProvider() override = default;

  // Getter for search engine images for unit test.
  UIImage* generic_search_icon() { return generic_search_icon_; }
  UIImage* google_search_icon() { return google_search_icon_; }

  // BrandedImageProvider implementation.
  UIImage* GetToolbarSearchButtonImage(SearchEngineIcon type) override {
    switch (type) {
      case SEARCH_ENGINE_ICON_GOOGLE_SEARCH:
        return google_search_icon_;

      default:
        return generic_search_icon_;
    }
  }

 private:
  UIImage* google_search_icon_;
  UIImage* generic_search_icon_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarMediatorBrandedImageProvider);
};

// Test ChromeBrowserProvider that install custom BrandedImageProvider and
// VoiceSearchProvider for ToolbarMediator unit tests.
class TestToolbarMediatorChromeBrowserProvider
    : public ios::TestChromeBrowserProvider {
 public:
  TestToolbarMediatorChromeBrowserProvider()
      : branded_image_provider_(
            std::make_unique<TestToolbarMediatorBrandedImageProvider>()),
        voice_search_provider_(
            std::make_unique<TestToolbarMediatorVoiceSearchProvider>()) {}

  ~TestToolbarMediatorChromeBrowserProvider() override = default;

  // ChromeBrowserProvider implementation.
  BrandedImageProvider* GetBrandedImageProvider() const override {
    return branded_image_provider_.get();
  }

  VoiceSearchProvider* GetVoiceSearchProvider() const override {
    return voice_search_provider_.get();
  }

 private:
  std::unique_ptr<BrandedImageProvider> branded_image_provider_;
  std::unique_ptr<VoiceSearchProvider> voice_search_provider_;

  DISALLOW_COPY_AND_ASSIGN(TestToolbarMediatorChromeBrowserProvider);
};
}

@interface TestToolbarMediator
    : ToolbarMediator<CRWWebStateObserver, WebStateListObserving>
@end

@implementation TestToolbarMediator
@end

namespace {

static const int kNumberOfWebStates = 3;
static const char kTestUrl[] = "http://www.chromium.org";
static const char kTestUrl2[] = "http://www.notChromium.org";

class ToolbarMediatorTest : public PlatformTest {
 public:
  ToolbarMediatorTest()
      : scoped_provider_(
            std::make_unique<TestToolbarMediatorChromeBrowserProvider>()) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    chrome_browser_state_->CreateBookmarkModel(false);
    bookmark_model_ = ios::BookmarkModelFactory::GetForBrowserState(
        chrome_browser_state_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
    std::unique_ptr<ToolbarTestNavigationManager> navigation_manager =
        std::make_unique<ToolbarTestNavigationManager>();
    navigation_manager_ = navigation_manager.get();
    test_web_state_ = std::make_unique<ToolbarTestWebState>();
    test_web_state_->SetNavigationManager(std::move(navigation_manager));
    test_web_state_->SetLoading(true);
    web_state_ = test_web_state_.get();
    mediator_ = [[TestToolbarMediator alloc] init];
    consumer_ = OCMProtocolMock(@protocol(ToolbarConsumer));
    strict_consumer_ = OCMStrictProtocolMock(@protocol(ToolbarConsumer));
    SetUpWebStateList();
    url_service_ = ios::TemplateURLServiceFactory::GetForBrowserState(
        chrome_browser_state_.get());
    SetDefaultSearchEngineGoogle();
  }

  // Explicitly disconnect the mediator so there won't be any WebStateList
  // observers when web_state_list_ gets dealloc.
  ~ToolbarMediatorTest() override { [mediator_ disconnect]; }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  void SetUpWebStateList() {
    web_state_list_ = std::make_unique<WebStateList>(&web_state_list_delegate_);
    web_state_list_->InsertWebState(0, std::move(test_web_state_),
                                    WebStateList::INSERT_FORCE_INDEX,
                                    WebStateOpener());
    for (int i = 1; i < kNumberOfWebStates; i++) {
      InsertNewWebState(i);
    }
  }

  void SetUpBookmarks() {
    bookmark_model_ = ios::BookmarkModelFactory::GetForBrowserState(
        chrome_browser_state_.get());
    GURL URL = GURL(kTestUrl);
    const BookmarkNode* defaultFolder = bookmark_model_->mobile_node();
    bookmark_model_->AddURL(defaultFolder, defaultFolder->child_count(),
                            base::SysNSStringToUTF16(@"Test bookmark 1"), URL);
  }

  void InsertNewWebState(int index) {
    auto web_state = std::make_unique<web::TestWebState>();
    GURL url("http://test/" + std::to_string(index));
    web_state->SetCurrentURL(url);
    web_state_list_->InsertWebState(index, std::move(web_state),
                                    WebStateList::INSERT_FORCE_INDEX,
                                    WebStateOpener());
  }

  void SetUpActiveWebState() { web_state_list_->ActivateWebStateAt(0); }

  // Sets the default search engine of the url_service to google.
  void SetDefaultSearchEngineGoogle() {
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("Google"));
    data.SetKeyword(base::ASCIIToUTF16("Google"));
    data.SetURL("http://google.com/?q={searchTerms}");
    TemplateURL* template_url =
        url_service_->Add(std::make_unique<TemplateURL>(data));
    url_service_->SetUserSelectedDefaultSearchProvider(template_url);
  }

  // Sets the default search engine of the url_service to not-google.
  void SetDefaultSearchEngineNotGoogle() {
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("TestEngine"));
    data.SetKeyword(base::ASCIIToUTF16("TestEngine"));
    data.SetURL("http://testsearch.com/?q={searchTerms}");
    TemplateURL* template_url =
        url_service_->Add(std::make_unique<TemplateURL>(data));
    url_service_->SetUserSelectedDefaultSearchProvider(template_url);
  }

  UIImage* google_search_icon() {
    return static_cast<TestToolbarMediatorBrandedImageProvider*>(
               ios::GetChromeBrowserProvider()->GetBrandedImageProvider())
        ->google_search_icon();
  }

  void set_voice_search_enabled(bool enabled) {
    static_cast<TestToolbarMediatorVoiceSearchProvider*>(
        ios::GetChromeBrowserProvider()->GetVoiceSearchProvider())
        ->set_voice_search_enabled(enabled);
  }

  IOSChromeScopedTestingChromeBrowserProvider scoped_provider_;
  TestToolbarMediator* mediator_;
  ToolbarTestWebState* web_state_;
  ToolbarTestNavigationManager* navigation_manager_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  id consumer_;
  id strict_consumer_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  TemplateURLService* url_service_;
  BookmarkModel* bookmark_model_;

 private:
  std::unique_ptr<ToolbarTestWebState> test_web_state_;
};

// Test that the consumer bookmarks status is only updated when the page is
// added to the bookmark model.
TEST_F(ToolbarMediatorTest, TestToolbarAddedToBookmarks) {
  web_state_->SetCurrentURL(GURL(kTestUrl));
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;
  mediator_.bookmarkModel = bookmark_model_;

  // Add a different bookmark and verify it is not set as bookmarked.
  OCMExpect([consumer_ setPageBookmarked:NO]);
  bookmark_model_ = ios::BookmarkModelFactory::GetForBrowserState(
      chrome_browser_state_.get());
  GURL URL = GURL(kTestUrl2);
  const BookmarkNode* defaultFolder = bookmark_model_->mobile_node();
  bookmark_model_->AddURL(defaultFolder, defaultFolder->child_count(),
                          base::SysNSStringToUTF16(@"Test bookmark 1"), URL);
  EXPECT_OCMOCK_VERIFY(consumer_);

  // Bookmark the page and check it is set.
  OCMExpect([consumer_ setPageBookmarked:YES]);
  bookmark_model_ = ios::BookmarkModelFactory::GetForBrowserState(
      chrome_browser_state_.get());
  URL = GURL(kTestUrl);
  bookmark_model_->AddURL(defaultFolder, defaultFolder->child_count(),
                          base::SysNSStringToUTF16(@"Test bookmark 2"), URL);

  EXPECT_OCMOCK_VERIFY(consumer_);
  bookmark_model_->RemoveAllUserBookmarks();
}

// Test that the consumer bookmarks status is only updated when the page is
// removed from the bookmark model.
TEST_F(ToolbarMediatorTest, TestToolbarRemovedFromBookmarks) {
  SetUpBookmarks();
  bookmark_model_ = ios::BookmarkModelFactory::GetForBrowserState(
      chrome_browser_state_.get());
  GURL URL = GURL(kTestUrl2);
  const BookmarkNode* defaultFolder = bookmark_model_->mobile_node();
  bookmark_model_->AddURL(defaultFolder, defaultFolder->child_count(),
                          base::SysNSStringToUTF16(@"Test bookmark 1"), URL);
  web_state_->SetCurrentURL(GURL(kTestUrl));
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;
  mediator_.bookmarkModel = bookmark_model_;

  // Removes another bookmark and check it is still bookmarked.
  OCMExpect([consumer_ setPageBookmarked:YES]);
  std::vector<const BookmarkNode*> vec;
  bookmark_model_->GetNodesByURL(GURL(kTestUrl2), &vec);
  bookmark_model_->Remove(vec.front());
  EXPECT_OCMOCK_VERIFY(consumer_);
  vec.clear();

  // Removes the page from the bookmarks and check it is updated.
  OCMExpect([consumer_ setPageBookmarked:NO]);
  bookmark_model_->GetNodesByURL(GURL(kTestUrl), &vec);
  bookmark_model_->Remove(vec.front());
  EXPECT_OCMOCK_VERIFY(consumer_);
  bookmark_model_->RemoveAllUserBookmarks();
}

// Test that the consumer bookmarks status is updated when the page is
// bookmarked.
TEST_F(ToolbarMediatorTest, TestToolbarBookmarked) {
  SetUpBookmarks();
  OCMExpect([consumer_ setPageBookmarked:YES]);

  web_state_->SetCurrentURL(GURL(kTestUrl));
  mediator_.bookmarkModel = bookmark_model_;
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
  bookmark_model_->RemoveAllUserBookmarks();
}

// Test that the consumer bookmarks status is updated when the page is
// bookmarked, when the bookmarkModel is set last.
TEST_F(ToolbarMediatorTest, TestToolbarBookmarkedModelSetLast) {
  SetUpBookmarks();
  OCMExpect([consumer_ setPageBookmarked:NO]);
  OCMExpect([consumer_ setShareMenuEnabled:YES]);

  web_state_->SetCurrentURL(GURL(kTestUrl));
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;
  mediator_.bookmarkModel = bookmark_model_;

  EXPECT_OCMOCK_VERIFY(consumer_);
  bookmark_model_->RemoveAllUserBookmarks();
}

// Test that the consumer bookmarks status is updated when the page is
// NOT bookmarked.
TEST_F(ToolbarMediatorTest, TestToolbarNotBookmarked) {
  SetUpBookmarks();
  OCMExpect([consumer_ setPageBookmarked:NO]);

  web_state_->SetCurrentURL(GURL(kTestUrl2));
  mediator_.bookmarkModel = bookmark_model_;
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
  bookmark_model_->RemoveAllUserBookmarks();
}

// Test no setup is being done on the Toolbar if there's no Webstate.
TEST_F(ToolbarMediatorTest, TestToolbarSetupWithNoWebstate) {
  mediator_.consumer = consumer_;

  [[consumer_ reject] setCanGoForward:NO];
  [[consumer_ reject] setCanGoBack:NO];
  [[consumer_ reject] setLoadingState:YES];
}

// Test no setup is being done on the Toolbar if there's no active Webstate.
TEST_F(ToolbarMediatorTest, TestToolbarSetupWithNoActiveWebstate) {
  SetUpBookmarks();
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;
  mediator_.bookmarkModel = bookmark_model_;

  [[consumer_ reject] setCanGoForward:NO];
  [[consumer_ reject] setCanGoBack:NO];
  [[consumer_ reject] setLoadingState:YES];
  [[consumer_ reject] setPageBookmarked:NO];
}

// Test no WebstateList related setup is being done on the Toolbar if there's no
// WebstateList.
TEST_F(ToolbarMediatorTest, TestToolbarSetupWithNoWebstateList) {
  mediator_.consumer = consumer_;

  [[[consumer_ reject] ignoringNonObjectArgs] setTabCount:0];
}

// Test the Toolbar Setup gets called when the mediator's WebState and Consumer
// have been set.
TEST_F(ToolbarMediatorTest, TestToolbarSetup) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.templateURLService = url_service_;
  mediator_.consumer = consumer_;

  [[consumer_ verify] setCanGoForward:NO];
  [[consumer_ verify] setCanGoBack:NO];
  [[consumer_ verify] setLoadingState:YES];
  [[consumer_ verify] setShareMenuEnabled:NO];
  [[consumer_ verify] setSearchIcon:google_search_icon()];
}

// Test the Toolbar Setup gets called when the mediator's WebState and Consumer
// have been set in reverse order.
TEST_F(ToolbarMediatorTest, TestToolbarSetupReverse) {
  mediator_.consumer = consumer_;
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.templateURLService = url_service_;

  [[consumer_ verify] setCanGoForward:NO];
  [[consumer_ verify] setCanGoBack:NO];
  [[consumer_ verify] setLoadingState:YES];
  [[consumer_ verify] setShareMenuEnabled:NO];
  [[consumer_ verify] setSearchIcon:google_search_icon()];
}

// Test the WebstateList related setup gets called when the mediator's WebState
// and Consumer have been set.
TEST_F(ToolbarMediatorTest, TestWebstateListRelatedSetup) {
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  [[consumer_ verify] setTabCount:3];
}

// Test the WebstateList related setup gets called when the mediator's WebState
// and Consumer have been set in reverse order.
TEST_F(ToolbarMediatorTest, TestWebstateListRelatedSetupReverse) {
  mediator_.consumer = consumer_;
  mediator_.webStateList = web_state_list_.get();

  [[consumer_ verify] setTabCount:3];
}

// Test the Toolbar is updated when the Webstate observer method DidStartLoading
// is triggered by SetLoading.
TEST_F(ToolbarMediatorTest, TestDidStartLoading) {
  // Change the default loading state to false to verify the Webstate
  // callback with true.
  web_state_->SetLoading(false);
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  web_state_->SetLoading(true);
  [[consumer_ verify] setLoadingState:YES];
}

// Test the Toolbar is updated when the Webstate observer method DidStopLoading
// is triggered by SetLoading.
TEST_F(ToolbarMediatorTest, TestDidStopLoading) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  web_state_->SetLoading(false);
  [[consumer_ verify] setLoadingState:NO];
}

// Test the Toolbar is updated when the Webstate observer method
// DidLoadPageWithSuccess is triggered by OnPageLoaded.
TEST_F(ToolbarMediatorTest, TestDidLoadPageWithSucess) {
  SetUpBookmarks();
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;
  mediator_.bookmarkModel = bookmark_model_;

  navigation_manager_->set_can_go_forward(true);
  navigation_manager_->set_can_go_back(true);

  web_state_->SetCurrentURL(GURL(kTestUrl));
  web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  [[consumer_ verify] setCanGoForward:YES];
  [[consumer_ verify] setCanGoBack:YES];
  [[consumer_ verify] setPageBookmarked:YES];
  [[consumer_ verify] setShareMenuEnabled:YES];
}

// Test the Toolbar is updated when the Webstate observer method
// didFinishNavigation is called.
TEST_F(ToolbarMediatorTest, TestDidFinishNavigation) {
  SetUpBookmarks();
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;
  mediator_.bookmarkModel = bookmark_model_;

  navigation_manager_->set_can_go_forward(true);
  navigation_manager_->set_can_go_back(true);

  web_state_->SetCurrentURL(GURL(kTestUrl));
  web::FakeNavigationContext context;
  web_state_->OnNavigationFinished(&context);

  [[consumer_ verify] setCanGoForward:YES];
  [[consumer_ verify] setCanGoBack:YES];
  [[consumer_ verify] setPageBookmarked:YES];
  [[consumer_ verify] setShareMenuEnabled:YES];
}

// Test the Toolbar is updated when the Webstate observer method
// didFinishNavigation is called.
TEST_F(ToolbarMediatorTest, TestDidChangeVisibleSecurityState) {
  SetUpBookmarks();
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;
  mediator_.bookmarkModel = bookmark_model_;

  navigation_manager_->set_can_go_forward(true);
  navigation_manager_->set_can_go_back(true);

  web_state_->SetCurrentURL(GURL(kTestUrl));
  web_state_->OnVisibleSecurityStateChanged();

  [[consumer_ verify] setCanGoForward:YES];
  [[consumer_ verify] setCanGoBack:YES];
  [[consumer_ verify] setPageBookmarked:YES];
  [[consumer_ verify] setShareMenuEnabled:YES];
}

// Test the Toolbar is updated when the Webstate observer method
// didChangeLoadingProgress is called.
TEST_F(ToolbarMediatorTest, TestLoadingProgress) {
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;

  [mediator_ webState:web_state_ didChangeLoadingProgress:0.42];
  [[consumer_ verify] setLoadingProgressFraction:0.42];
}

// Test that increasing the number of Webstates will update the consumer with
// the right value.
TEST_F(ToolbarMediatorTest, TestIncreaseNumberOfWebstates) {
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  InsertNewWebState(0);
  [[consumer_ verify] setTabCount:kNumberOfWebStates + 1];
}

// Test that decreasing the number of Webstates will update the consumer with
// the right value.
TEST_F(ToolbarMediatorTest, TestDecreaseNumberOfWebstates) {
  mediator_.webStateList = web_state_list_.get();
  mediator_.consumer = consumer_;

  web_state_list_->DetachWebStateAt(0);
  [[consumer_ verify] setTabCount:kNumberOfWebStates - 1];
}

// Test that consumer is informed that voice search is enabled.
TEST_F(ToolbarMediatorTest, TestVoiceSearchProviderEnabled) {
  set_voice_search_enabled(true);

  OCMExpect([consumer_ setVoiceSearchEnabled:YES]);
  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Test that consumer is informed that voice search is not enabled.
TEST_F(ToolbarMediatorTest, TestVoiceSearchProviderNotEnabled) {
  set_voice_search_enabled(false);

  OCMExpect([consumer_ setVoiceSearchEnabled:NO]);
  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Test that updating the consumer for a specific webState works.
TEST_F(ToolbarMediatorTest, TestUpdateConsumerForWebState) {
  VoiceSearchProvider provider;

  SetUpBookmarks();
  mediator_.webStateList = web_state_list_.get();
  SetUpActiveWebState();
  mediator_.consumer = consumer_;
  mediator_.bookmarkModel = bookmark_model_;

  auto navigation_manager = std::make_unique<ToolbarTestNavigationManager>();
  navigation_manager->set_can_go_forward(true);
  navigation_manager->set_can_go_back(true);
  std::unique_ptr<ToolbarTestWebState> test_web_state =
      std::make_unique<ToolbarTestWebState>();
  test_web_state->SetNavigationManager(std::move(navigation_manager));
  test_web_state->SetCurrentURL(GURL(kTestUrl));
  test_web_state->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  OCMExpect([consumer_ setCanGoForward:YES]);
  OCMExpect([consumer_ setCanGoBack:YES]);
  OCMExpect([consumer_ setPageBookmarked:YES]);
  OCMExpect([consumer_ setShareMenuEnabled:YES]);

  [mediator_ updateConsumerForWebState:test_web_state.get()];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that setting the image provider and template service with a default
// search engine different from google provide a non-nil image.
TEST_F(ToolbarMediatorTest, TestSetConsumerDefaultSearchEngineNotGoogle) {
  SetDefaultSearchEngineNotGoogle();
  OCMExpect([consumer_ setSearchIcon:[OCMArg isNotNil]]);

  mediator_.consumer = consumer_;
  mediator_.templateURLService = url_service_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that changing the search engine to google gives the image from the
// image provider.
TEST_F(ToolbarMediatorTest, TestChangeSearchEngineToGoogle) {
  SetDefaultSearchEngineNotGoogle();
  mediator_.consumer = consumer_;
  mediator_.templateURLService = url_service_;

  OCMExpect([consumer_ setSearchIcon:google_search_icon()]);

  SetDefaultSearchEngineGoogle();

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that changing the search engine to not-google gives an image different
// from the image provider's one.
TEST_F(ToolbarMediatorTest, TestChangeSearchEngineToNotGoogle) {
  SetDefaultSearchEngineGoogle();
  mediator_.consumer = consumer_;
  mediator_.templateURLService = url_service_;

  OCMExpect([consumer_ setSearchIcon:[OCMArg isNotEqual:google_search_icon()]]);

  SetDefaultSearchEngineNotGoogle();

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that having a nil image provider still gives an image.
TEST_F(ToolbarMediatorTest, TestSetConsumerNoImageProvider) {
  SetDefaultSearchEngineGoogle();
  OCMExpect([consumer_ setSearchIcon:[OCMArg isNotNil]]);
  mediator_.consumer = consumer_;
  mediator_.templateURLService = url_service_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that having an image provider returning a nil image still gives an
// image.
TEST_F(ToolbarMediatorTest, TestSetConsumerImageProviderNoImage) {
  SetDefaultSearchEngineGoogle();
  OCMExpect([consumer_ setSearchIcon:[OCMArg isNotNil]]);
  mediator_.templateURLService = url_service_;
  mediator_.consumer = consumer_;

  EXPECT_OCMOCK_VERIFY(consumer_);
}

}  // namespace
