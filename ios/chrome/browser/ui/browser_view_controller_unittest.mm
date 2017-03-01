// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/toolbar/test_toolbar_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/activity_services/share_protocol.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/browser_view_controller_testing.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#import "ios/chrome/browser/ui/page_not_available_controller.h"
#include "ios/chrome/browser/ui/toolbar/test_toolbar_model_ios.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/web/error_page_content.h"
#import "ios/chrome/browser/web/passkit_dialog_provider.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/chrome/test/testing_application_context.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/navigation/crw_session_controller.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/test/ios/ui_image_test_utils.h"

using web::NavigationManagerImpl;
using web::WebStateImpl;

// Private methods in BrowserViewController to test.
@interface BrowserViewController (
    Testing)<CRWNativeContentProvider, PassKitDialogProvider, ShareToDelegate>
- (void)pageLoadStarted:(NSNotification*)notification;
- (void)pageLoadComplete:(NSNotification*)notification;
- (void)tabSelected:(Tab*)tab;
- (void)tabDeselected:(NSNotification*)notification;
- (void)tabCountChanged:(NSNotification*)notification;
- (IBAction)chromeExecuteCommand:(id)sender;
@end

@interface BVCTestTabMock : OCMockComplexTypeHelper {
  GURL url_;
  WebStateImpl* webState_;
}

@property(nonatomic, assign) const GURL& url;
@property(nonatomic, assign) WebStateImpl* webState;

- (web::NavigationManager*)navigationManager;
- (web::NavigationManagerImpl*)navigationManagerImpl;

@end

@implementation BVCTestTabMock
- (const GURL&)url {
  return url_;
}
- (void)setUrl:(const GURL&)url {
  url_ = url;
}
- (WebStateImpl*)webState {
  return webState_;
}
- (void)setWebState:(WebStateImpl*)webState {
  webState_ = webState;
}
- (web::NavigationManager*)navigationManager {
  return &(webState_->GetNavigationManagerImpl());
}
- (web::NavigationManagerImpl*)navigationManagerImpl {
  return &(webState_->GetNavigationManagerImpl());
}
@end

#pragma mark -

namespace {
class BrowserViewControllerTest : public BlockCleanupTest {
 public:
  BrowserViewControllerTest() {}

 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    // Disable Contextual Search on the command line.
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableContextualSearch)) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kDisableContextualSearch);
    }

    // Set up a TestChromeBrowserState instance.
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    chrome_browser_state_ = test_cbs_builder.Build();
    chrome_browser_state_->CreateBookmarkModel(false);
    bookmarks::BookmarkModel* bookmark_model =
        ios::BookmarkModelFactory::GetForBrowserState(
            chrome_browser_state_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);

    // Set up mock TabModel, Tab, and CRWWebController.
    id tabModel = [OCMockObject mockForClass:[TabModel class]];
    base::scoped_nsobject<id> currentTab([[BVCTestTabMock alloc]
        initWithRepresentedObject:[OCMockObject niceMockForClass:[Tab class]]]);
    id webControllerMock =
        [OCMockObject niceMockForClass:[CRWWebController class]];

    // Stub methods for TabModel.
    NSUInteger tabCount = 1;
    [[[tabModel stub] andReturnValue:OCMOCK_VALUE(tabCount)] count];
    BOOL enabled = YES;
    [[[tabModel stub] andReturnValue:OCMOCK_VALUE(enabled)] webUsageEnabled];
    [[[tabModel stub] andReturn:currentTab] currentTab];
    [[[tabModel stub] andReturn:currentTab] tabAtIndex:0];
    [[tabModel stub] addObserver:[OCMArg any]];
    [[tabModel stub] removeObserver:[OCMArg any]];
    [[tabModel stub] saveSessionImmediately:NO];
    [[tabModel stub] setCurrentTab:[OCMArg any]];
    [[tabModel stub] closeAllTabs];

    // Stub methods for Tab.
    UIView* dummyView = [[[UIView alloc] initWithFrame:CGRectZero] autorelease];
    [[[currentTab stub] andReturn:dummyView] view];
    [[[currentTab stub] andReturn:webControllerMock] webController];

    id sessionControllerMock =
        [OCMockObject niceMockForClass:[CRWSessionController class]];
    webStateImpl_.reset(new WebStateImpl(chrome_browser_state_.get()));
    [currentTab setWebState:webStateImpl_.get()];
    webStateImpl_->SetWebController(webControllerMock);
    webStateImpl_->GetNavigationManagerImpl().SetSessionController(
        sessionControllerMock);

    // Set up mock ShareController.
    id shareController =
        [OCMockObject niceMockForProtocol:@protocol(ShareProtocol)];
    shareController_.reset([shareController retain]);

    id passKitController =
        [OCMockObject niceMockForClass:[PKAddPassesViewController class]];
    passKitViewController_.reset([passKitController retain]);

    // Set up a fake toolbar model for the dependency factory to return.
    // It will be owned (and destroyed) by the BVC.
    toolbarModelIOS_ = new TestToolbarModelIOS();

    // Set up a stub dependency factory.
    id factory = [OCMockObject
        mockForClass:[BrowserViewControllerDependencyFactory class]];
    [[[factory stub] andReturn:nil]
        newTabStripControllerWithTabModel:[OCMArg any]];
    [[[factory stub] andReturn:nil] newPreloadController];
    [[[factory stub] andReturnValue:OCMOCK_VALUE(toolbarModelIOS_)]
        newToolbarModelIOSWithDelegate:static_cast<ToolbarModelDelegateIOS*>(
                                           [OCMArg anyPointer])];
    [[[factory stub] andReturn:nil]
        newWebToolbarControllerWithDelegate:[OCMArg any]
                                  urlLoader:[OCMArg any]
                            preloadProvider:[OCMArg any]];
    [[[factory stub] andReturn:shareController_.get()] shareControllerInstance];
    [[[factory stub] andReturn:passKitViewController_.get()]
        newPassKitViewControllerForPass:nil];
    [[[factory stub] andReturn:nil] showPassKitErrorInfoBarForManager:nil];

    webController_.reset([webControllerMock retain]);
    tabModel_.reset([tabModel retain]);
    tab_.reset([currentTab retain]);
    dependencyFactory_.reset([factory retain]);
    bvc_.reset([[BrowserViewController alloc]
         initWithTabModel:tabModel_
             browserState:chrome_browser_state_.get()
        dependencyFactory:factory]);

    // Load TemplateURLService.
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    template_url_service->Load();

    // Force the view to load.
    UIWindow* window = [[UIWindow alloc] initWithFrame:CGRectZero];
    [window addSubview:[bvc_ view]];
    window_.reset(window);
  }

  void TearDown() override {
    [[bvc_ view] removeFromSuperview];
    BlockCleanupTest::TearDown();
  }

  base::scoped_nsobject<GenericChromeCommand> GetCommandWithTag(NSInteger tag) {
    base::scoped_nsobject<GenericChromeCommand> command(
        [[GenericChromeCommand alloc] initWithTag:tag]);
    return command;
  }

  MOCK_METHOD0(OnCompletionCalled, void());

  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<WebStateImpl> webStateImpl_;
  base::scoped_nsobject<CRWWebController> webController_;
  base::scoped_nsobject<Tab> tab_;
  base::scoped_nsobject<TabModel> tabModel_;
  ToolbarModelIOS* toolbarModelIOS_;
  base::scoped_nsprotocol<id<ShareProtocol>> shareController_;
  base::scoped_nsobject<PKAddPassesViewController> passKitViewController_;
  base::scoped_nsobject<OCMockObject> dependencyFactory_;
  base::scoped_nsobject<BrowserViewController> bvc_;
  base::scoped_nsobject<UIWindow> window_;
};

// TODO(crbug.com/228714): These tests pretty much only tested that BVC passed
// notifications on to the toolbar, and that the toolbar worked correctly. The
// former should be an integration test, and the latter should be a toolbar
// test.  Leaving DISABLED_ for now to remind us to move them to toolbar tests.
TEST_F(BrowserViewControllerTest, DISABLED_TestPageLoadStarted) {
  NSDictionary* userInfoWithThisTab =
      [NSDictionary dictionaryWithObject:tab_ forKey:kTabModelTabKey];
  NSNotification* notification = [NSNotification
      notificationWithName:kTabModelTabWillStartLoadingNotification
                    object:nil
                  userInfo:userInfoWithThisTab];
  [bvc_ pageLoadStarted:notification];
  EXPECT_TRUE([bvc_ testing_isLoading]);
}

TEST_F(BrowserViewControllerTest, DISABLED_TestPageLoadComplete) {
  NSDictionary* userInfoWithThisTab =
      [NSDictionary dictionaryWithObject:tab_ forKey:kTabModelTabKey];
  NSNotification* notification = [NSNotification
      notificationWithName:kTabModelTabDidFinishLoadingNotification
                    object:nil
                  userInfo:userInfoWithThisTab];
  [bvc_ pageLoadComplete:notification];
  EXPECT_FALSE([bvc_ testing_isLoading]);
}

TEST_F(BrowserViewControllerTest, TestTabSelected) {
  id tabMock = (id)tab_.get();
  [[tabMock expect] wasShown];
  [bvc_ tabSelected:tab_];
  EXPECT_EQ([[tab_ view] superview], static_cast<UIView*>([bvc_ contentArea]));
  EXPECT_OCMOCK_VERIFY(tabMock);
}

TEST_F(BrowserViewControllerTest, TestTabSelectedIsNewTab) {
  base::scoped_nsobject<id> block([^{
    return GURL(kChromeUINewTabURL);
  } copy]);
  id tabMock = (id)tab_.get();
  [tabMock onSelector:@selector(url) callBlockExpectation:block];
  [[tabMock expect] wasShown];
  [bvc_ tabSelected:tab_];
  EXPECT_EQ([[tab_ view] superview], static_cast<UIView*>([bvc_ contentArea]));
  EXPECT_OCMOCK_VERIFY(tabMock);
}

TEST_F(BrowserViewControllerTest, TestTabDeselected) {
  OCMockObject* tabMock = static_cast<OCMockObject*>(tab_.get());
  [[tabMock expect] wasHidden];
  NSDictionary* userInfoWithThisTab =
      [NSDictionary dictionaryWithObject:tab_ forKey:kTabModelTabKey];
  NSNotification* notification =
      [NSNotification notificationWithName:kTabModelTabDeselectedNotification
                                    object:nil
                                  userInfo:userInfoWithThisTab];
  [bvc_ tabDeselected:notification];
  EXPECT_OCMOCK_VERIFY(tabMock);
}

TEST_F(BrowserViewControllerTest, TestNativeContentController) {
  id<CRWNativeContent> controller =
      [bvc_ controllerForURL:GURL(kChromeUINewTabURL)
                    webState:webStateImpl_.get()];
  EXPECT_TRUE(controller != nil);
  EXPECT_TRUE([controller isMemberOfClass:[NewTabPageController class]]);

  controller = [bvc_ controllerForURL:GURL(kChromeUISettingsURL)
                             webState:webStateImpl_.get()];
  EXPECT_TRUE(controller != nil);
  EXPECT_TRUE([controller isMemberOfClass:[PageNotAvailableController class]]);
}

TEST_F(BrowserViewControllerTest, TestErrorController) {
  const GURL badUrl("http://floofywhizbangzzz.com");
  NSString* badURLString = base::SysUTF8ToNSString(badUrl.spec());
  NSDictionary* userInfoDic = [NSDictionary
      dictionaryWithObjectsAndKeys:badURLString,
                                   NSURLErrorFailingURLStringErrorKey,
                                   [NSError
                                       errorWithDomain:base::SysUTF8ToNSString(
                                                           net::kErrorDomain)
                                                  code:-104
                                              userInfo:nil],
                                   NSUnderlyingErrorKey, nil];
  NSError* testError =
      [NSError errorWithDomain:@"testdomain" code:-1 userInfo:userInfoDic];
  id<CRWNativeContent> controller =
      [bvc_ controllerForURL:badUrl withError:testError isPost:NO];
  EXPECT_TRUE(controller != nil);
  EXPECT_TRUE([controller isMemberOfClass:[ErrorPageContent class]]);
}

// TODO(altse): Needs a testing |Profile| that implements AutocompleteClassifier
//             before enabling again.
TEST_F(BrowserViewControllerTest, DISABLED_TestShieldWasTapped) {
  [bvc_ testing_focusOmnibox];
  EXPECT_TRUE([[bvc_ typingShield] superview] != nil);
  EXPECT_FALSE([[bvc_ typingShield] isHidden]);
  [bvc_ shieldWasTapped:nil];
  EXPECT_TRUE([[bvc_ typingShield] superview] == nil);
  EXPECT_TRUE([[bvc_ typingShield] isHidden]);
}

// Verifies that editing the omnimbox while the page is loading will stop the
// load on a handset, but not stop the load on a tablet.
TEST_F(BrowserViewControllerTest,
       TestLocationBarBeganEdit_whenPageLoadIsInProgress) {
  OCMockObject* tabMock = static_cast<OCMockObject*>(tab_.get());

  // Have the TestToolbarModel indicate that a page load is in progress.
  static_cast<TestToolbarModelIOS*>(toolbarModelIOS_)->set_is_loading(true);

  // The tab should only stop loading on handsets.
  if (!IsIPadIdiom())
    [[tabMock expect] stopLoading];
  [bvc_ locationBarBeganEdit:nil];

  EXPECT_OCMOCK_VERIFY(tabMock);
}

// Verifies that editing the omnibox when the page is not loading will not try
// to stop the load on a handset or a tablet.
TEST_F(BrowserViewControllerTest,
       TestLocationBarBeganEdit_whenPageLoadIsComplete) {
  OCMockObject* tabMock = static_cast<OCMockObject*>(tab_.get());

  // Have the TestToolbarModel indicate that the page load is complete.
  static_cast<TestToolbarModelIOS*>(toolbarModelIOS_)->set_is_loading(false);

  // Don't set any expectation for stopLoading to be called on the mock tab.
  [bvc_ locationBarBeganEdit:nil];

  EXPECT_OCMOCK_VERIFY(tabMock);
}

// Verifies that BVC invokes -shareURL on ShareController with the correct
// parameters in response to the IDC_SHARE_PAGE command.
TEST_F(BrowserViewControllerTest, TestSharePageCommandHandling) {
  GURL expectedUrl("http://www.testurl.net");
  NSString* expectedTitle = @"title";
  [static_cast<BVCTestTabMock*>(tab_.get()) setUrl:expectedUrl];
  OCMockObject* tabMock = static_cast<OCMockObject*>(tab_.get());
  ios::ChromeBrowserState* ptr = chrome_browser_state_.get();
  [[[tabMock stub] andReturnValue:OCMOCK_VALUE(ptr)] browserState];
  [[[tabMock stub] andReturn:expectedTitle] title];
  [[[tabMock stub] andReturn:expectedTitle] originalTitle];

  UIImage* tabSnapshot = ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
      CGSizeMake(300, 400), [UIColor blueColor]);
  [[[tabMock stub] andReturn:tabSnapshot] generateSnapshotWithOverlay:NO
                                                     visibleFrameOnly:YES];
  OCMockObject* shareControllerMock =
      static_cast<OCMockObject*>(shareController_.get());
  // Passing non zero/nil |fromRect| and |inView| parameters to satisfy protocol
  // requirements.
  BOOL (^shareDataChecker)
  (id value) = ^BOOL(id value) {
    if (![value isMemberOfClass:ShareToData.class])
      return NO;
    ShareToData* shareToData = static_cast<ShareToData*>(value);
    CGSize size = CGSizeMake(40, 40);
    BOOL thumbnailDataIsEqual = ui::test::uiimage_utils::UIImagesAreEqual(
        shareToData.thumbnailGenerator(size),
        ui::test::uiimage_utils::UIImageWithSizeAndSolidColor(
            size, [UIColor blueColor]));
    return shareToData.url == expectedUrl &&
           [shareToData.title isEqual:expectedTitle] &&
           shareToData.isOriginalTitle == YES &&
           shareToData.isPagePrintable == NO && thumbnailDataIsEqual;
  };

  [[shareControllerMock expect]
        shareWithData:[OCMArg checkWithBlock:shareDataChecker]
           controller:bvc_
         browserState:chrome_browser_state_.get()
      shareToDelegate:bvc_
             fromRect:[bvc_ testing_shareButtonAnchorRect]
               inView:[OCMArg any]];
  [bvc_ chromeExecuteCommand:GetCommandWithTag(IDC_SHARE_PAGE)];
  EXPECT_OCMOCK_VERIFY(shareControllerMock);
}

// Verifies that BVC does not invoke -shareURL on ShareController in response
// to the IDC_SHARE_PAGE command if tab is in the process of being closed.
TEST_F(BrowserViewControllerTest, TestSharePageWhenClosing) {
  GURL expectedUrl("http://www.testurl.net");
  NSString* expectedTitle = @"title";
  // Sets WebState to nil because [tab close] clears the WebState.
  [static_cast<BVCTestTabMock*>(tab_.get()) setWebState:nil];
  [static_cast<BVCTestTabMock*>(tab_.get()) setUrl:expectedUrl];
  OCMockObject* tabMock = static_cast<OCMockObject*>(tab_.get());
  [[[tabMock stub] andReturn:expectedTitle] title];
  [[[tabMock stub] andReturn:expectedTitle] originalTitle];
  // Explicitly disallow the execution of the ShareController.
  OCMockObject* shareControllerMock =
      static_cast<OCMockObject*>(shareController_.get());
  [[shareControllerMock reject]
        shareWithData:[OCMArg any]
           controller:bvc_
         browserState:chrome_browser_state_.get()
      shareToDelegate:bvc_
             fromRect:[bvc_ testing_shareButtonAnchorRect]
               inView:[OCMArg any]];
  [bvc_ chromeExecuteCommand:GetCommandWithTag(IDC_SHARE_PAGE)];
  EXPECT_OCMOCK_VERIFY(shareControllerMock);
}

// Verifies that BVC instantiates a bubble to show the given success message on
// receiving a -shareDidComplete callback for a successful share.
TEST_F(BrowserViewControllerTest, TestShareDidCompleteWithSuccess) {
  NSString* successMessage = @"Success";
  [[dependencyFactory_ expect] showSnackbarWithMessage:successMessage];

  [bvc_ shareDidComplete:ShareTo::SHARE_SUCCESS successMessage:successMessage];
  EXPECT_OCMOCK_VERIFY(dependencyFactory_);
}

// Verifies that BVC shows an alert with the proper error message on
// receiving a -shareDidComplete callback for a failed share.
TEST_F(BrowserViewControllerTest, TestShareDidCompleteWithError) {
  [[dependencyFactory_ reject] showSnackbarWithMessage:OCMOCK_ANY];
  base::scoped_nsobject<OCMockObject> mockCoordinator(
      [[OCMockObject niceMockForClass:[AlertCoordinator class]] retain]);
  AlertCoordinator* alertCoordinator =
      static_cast<AlertCoordinator*>(mockCoordinator.get());
  NSString* errorTitle =
      l10n_util::GetNSString(IDS_IOS_SHARE_TO_ERROR_ALERT_TITLE);
  NSString* errorMessage = l10n_util::GetNSString(IDS_IOS_SHARE_TO_ERROR_ALERT);
  [[[dependencyFactory_ expect] andReturn:alertCoordinator]
      alertCoordinatorWithTitle:errorTitle
                        message:errorMessage
                 viewController:OCMOCK_ANY];
  [static_cast<AlertCoordinator*>([mockCoordinator expect]) start];

  [bvc_ shareDidComplete:ShareTo::SHARE_ERROR successMessage:@"dummy"];
  EXPECT_OCMOCK_VERIFY(dependencyFactory_);
  EXPECT_OCMOCK_VERIFY(mockCoordinator);
}

// Verifies that BVC does not show a success bubble or error alert on receiving
// a -shareDidComplete callback for a cancelled share.
TEST_F(BrowserViewControllerTest, TestShareDidCompleteWithCancellation) {
  [[dependencyFactory_ reject] showSnackbarWithMessage:OCMOCK_ANY];
  [[dependencyFactory_ reject] alertCoordinatorWithTitle:OCMOCK_ANY
                                                 message:OCMOCK_ANY
                                          viewController:OCMOCK_ANY];

  [bvc_ shareDidComplete:ShareTo::SHARE_CANCEL successMessage:@"dummy"];
  EXPECT_OCMOCK_VERIFY(dependencyFactory_);
}

TEST_F(BrowserViewControllerTest, TestPassKitDialogDisplayed) {
  // Create a good Pass and make sure the controller is displayed.
  base::FilePath pass_path;
  ASSERT_TRUE(PathService::Get(ios::DIR_TEST_DATA, &pass_path));
  pass_path = pass_path.Append(FILE_PATH_LITERAL("testpass.pkpass"));
  NSData* passKitObject = [NSData
      dataWithContentsOfFile:base::SysUTF8ToNSString(pass_path.value())];
  EXPECT_TRUE(passKitObject);
  [[dependencyFactory_ expect] newPassKitViewControllerForPass:OCMOCK_ANY];
  [bvc_ presentPassKitDialog:passKitObject];
  EXPECT_OCMOCK_VERIFY(dependencyFactory_);
}

TEST_F(BrowserViewControllerTest, TestPassKitErrorInfoBarDisplayed) {
  // Create a bad Pass and make sure the controller is not displayed.
  base::FilePath bad_pass_path;
  ASSERT_TRUE(PathService::Get(ios::DIR_TEST_DATA, &bad_pass_path));
  bad_pass_path = bad_pass_path.Append(FILE_PATH_LITERAL("testbadpass.pkpass"));
  NSData* badPassKitObject = [NSData
      dataWithContentsOfFile:base::SysUTF8ToNSString(bad_pass_path.value())];
  EXPECT_TRUE(badPassKitObject);
  [[dependencyFactory_ reject] newPassKitViewControllerForPass:OCMOCK_ANY];
  [bvc_ presentPassKitDialog:badPassKitObject];
  EXPECT_OCMOCK_VERIFY(dependencyFactory_);
}

TEST_F(BrowserViewControllerTest, TestClearPresentedState) {
  OCMockObject* shareControllerMock =
      static_cast<OCMockObject*>(shareController_.get());
  [[shareControllerMock expect] cancelShareAnimated:NO];
  EXPECT_CALL(*this, OnCompletionCalled());
  [bvc_ clearPresentedStateWithCompletion:^{
    this->OnCompletionCalled();
  }];
  EXPECT_OCMOCK_VERIFY(shareControllerMock);
}

}  // namespace
