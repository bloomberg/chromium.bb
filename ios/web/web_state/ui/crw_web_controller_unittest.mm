// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <WebKit/WebKit.h>

#include <utility>

#include "base/callback_helpers.h"
#include "base/ios/ios_util.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/histogram_tester.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#include "ios/web/navigation/crw_session_controller.h"
#include "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/test/test_web_view_content_view.h"
#include "ios/web/public/test/web_test_util.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#import "ios/web/test/web_test.h"
#import "ios/web/test/wk_web_view_crash_utils.h"
#include "ios/web/web_state/blocked_popup_info.h"
#import "ios/web/web_state/js/crw_js_invoke_parameter_queue.h"
#import "ios/web/web_state/ui/crw_ui_web_view_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller+protected.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/base/test_data_directory.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/test/ios/keyboard_appearance_listener.h"
#include "ui/base/test/ios/ui_view_test_utils.h"

using web::NavigationManagerImpl;

@interface CRWWebController (PrivateAPI)
@property(nonatomic, readwrite) web::PageDisplayState pageDisplayState;
@property(nonatomic, readonly) CRWWebControllerContainerView* containerView;
- (void)setJsMessageQueueThrottled:(BOOL)throttle;
- (void)removeDocumentLoadCommandsFromQueue;
- (GURL)URLForHistoryNavigationFromItem:(web::NavigationItem*)fromItem
                                 toItem:(web::NavigationItem*)toItem;
@end

// Used to mock CRWWebDelegate methods with C++ params.
@interface MockInteractionLoader : OCMockComplexTypeHelper
// popupURL passed to webController:shouldBlockPopupWithURL:sourceURL:
// Used for testing.
@property(nonatomic, assign) GURL popupURL;
// sourceURL passed to webController:shouldBlockPopupWithURL:sourceURL:
// Used for testing.
@property(nonatomic, assign) GURL sourceURL;
// Whether or not the delegate should block popups.
@property(nonatomic, assign) BOOL blockPopups;
// A web controller that will be returned by webPageOrdered... methods.
@property(nonatomic, assign) CRWWebController* childWebController;
// Blocked popup info received in |webController:didBlockPopup:| call.
// nullptr if that delegate method was not called.
@property(nonatomic, readonly) web::BlockedPopupInfo* blockedPopupInfo;
// SSL info received in |presentSSLError:forSSLStatus:recoverable:callback:|
// call.
@property(nonatomic, readonly) net::SSLInfo SSLInfo;
// SSL status received in |presentSSLError:forSSLStatus:recoverable:callback:|
// call.
@property(nonatomic, readonly) web::SSLStatus SSLStatus;
// Recoverable flag received in
// |presentSSLError:forSSLStatus:recoverable:callback:| call.
@property(nonatomic, readonly) BOOL recoverable;
// Callback received in |presentSSLError:forSSLStatus:recoverable:callback:|
// call.
@property(nonatomic, readonly) SSLErrorCallback shouldContinueCallback;
@end

@implementation MockInteractionLoader {
  // Backs up the property with the same name.
  scoped_ptr<web::BlockedPopupInfo> _blockedPopupInfo;
}
@synthesize popupURL = _popupURL;
@synthesize sourceURL = _sourceURL;
@synthesize blockPopups = _blockPopups;
@synthesize childWebController = _childWebController;
@synthesize SSLInfo = _SSLInfo;
@synthesize SSLStatus = _SSLStatus;
@synthesize recoverable = _recoverable;
@synthesize shouldContinueCallback = _shouldContinueCallback;

typedef void (^webPageOrderedOpenBlankBlockType)();
typedef void (^webPageOrderedOpenBlockType)(const GURL&,
                                            const web::Referrer&,
                                            NSString*,
                                            BOOL);

- (instancetype)initWithRepresentedObject:(id)representedObject {
  self = [super initWithRepresentedObject:representedObject];
  if (self) {
    _blockPopups = YES;
  }
  return self;
}

- (CRWWebController*)webPageOrderedOpen {
  static_cast<webPageOrderedOpenBlankBlockType>([self blockForSelector:_cmd])();
  return _childWebController;
}

- (CRWWebController*)webPageOrderedOpen:(const GURL&)url
                               referrer:(const web::Referrer&)referrer
                             windowName:(NSString*)windowName
                           inBackground:(BOOL)inBackground {
  static_cast<webPageOrderedOpenBlockType>([self blockForSelector:_cmd])(
      url, referrer, windowName, inBackground);
  return _childWebController;
}

typedef BOOL (^openExternalURLBlockType)(const GURL&);

- (BOOL)openExternalURL:(const GURL&)url {
  return static_cast<openExternalURLBlockType>([self blockForSelector:_cmd])(
      url);
}

- (BOOL)webController:(CRWWebController*)webController
    shouldBlockPopupWithURL:(const GURL&)popupURL
                  sourceURL:(const GURL&)sourceURL {
  self.popupURL = popupURL;
  self.sourceURL = sourceURL;
  return _blockPopups;
}

- (void)webController:(CRWWebController*)webController
        didBlockPopup:(const web::BlockedPopupInfo&)blockedPopupInfo {
  _blockedPopupInfo.reset(new web::BlockedPopupInfo(blockedPopupInfo));
}

- (web::BlockedPopupInfo*)blockedPopupInfo {
  return _blockedPopupInfo.get();
}

- (void)presentSSLError:(const net::SSLInfo&)info
           forSSLStatus:(const web::SSLStatus&)status
            recoverable:(BOOL)recoverable
               callback:(SSLErrorCallback)shouldContinue {
  _SSLInfo = info;
  _SSLStatus = status;
  _recoverable = recoverable;
  _shouldContinueCallback = shouldContinue;
}

@end

@interface CountingObserver : NSObject<CRWWebControllerObserver>

@property(nonatomic, readonly) int pageLoadedCount;
@property(nonatomic, readonly) int messageCount;
@end

@implementation CountingObserver
@synthesize pageLoadedCount = _pageLoadedCount;
@synthesize messageCount = _messageCount;

- (void)pageLoaded:(CRWWebController*)webController {
  ++_pageLoadedCount;
}

- (BOOL)handleCommand:(const base::DictionaryValue&)command
        webController:(CRWWebController*)webController
    userIsInteracting:(BOOL)userIsInteracting
            originURL:(const GURL&)originURL {
  ++_messageCount;
  return YES;
}

- (NSString*)commandPrefix {
  return @"wctest";
}

@end

namespace {

NSString* kTestURLString = @"http://www.google.com/";

// Returns true if the current device is a large iPhone (6 or 6+).
bool IsIPhone6Or6Plus() {
  UIUserInterfaceIdiom idiom = [[UIDevice currentDevice] userInterfaceIdiom];
  return (idiom == UIUserInterfaceIdiomPhone &&
          CGRectGetHeight([[UIScreen mainScreen] nativeBounds]) >= 1334.0);
}

// Returns HTML for an optionally zoomable test page with |zoom_state|.
enum PageScalabilityType {
  PAGE_SCALABILITY_DISABLED = 0,
  PAGE_SCALABILITY_ENABLED,
};
NSString* GetHTMLForZoomState(const web::PageZoomState& zoom_state,
                              PageScalabilityType scalability_type) {
  NSString* const kHTMLFormat =
      @"<html><head><meta name='viewport' content="
       "'width=%f,minimum-scale=%f,maximum-scale=%f,initial-scale=%f,"
       "user-scalable=%@'/></head><body>Test</body></html>";
  CGFloat width = CGRectGetWidth([UIScreen mainScreen].bounds) /
      zoom_state.minimum_zoom_scale();
  BOOL scalability_enabled = scalability_type == PAGE_SCALABILITY_ENABLED;
  return [NSString
      stringWithFormat:kHTMLFormat, width, zoom_state.minimum_zoom_scale(),
                       zoom_state.maximum_zoom_scale(), zoom_state.zoom_scale(),
                       scalability_enabled ? @"yes" : @"no"];
}

// Forces |webController|'s view to render and waits until |webController|'s
// PageZoomState matches |zoom_state|.
void WaitForZoomRendering(CRWWebController* webController,
                          const web::PageZoomState& zoom_state) {
  ui::test::uiview_utils::ForceViewRendering(webController.view);
  base::test::ios::WaitUntilCondition(^bool() {
    return webController.pageDisplayState.zoom_state() == zoom_state;
  });
}

// Test fixture for testing CRWWebController. Stubs out web view and
// child CRWWebController.
class CRWWebControllerTest : public web::WebTestWithWKWebViewWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWKWebViewWebController::SetUp();
    mockWebView_.reset(CreateMockWebView());
    mockScrollView_.reset([[UIScrollView alloc] init]);
    [[[mockWebView_ stub] andReturn:mockScrollView_.get()] scrollView];

    id originalMockDelegate =
        [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
    mockDelegate_.reset([[MockInteractionLoader alloc]
        initWithRepresentedObject:originalMockDelegate]);
    [webController_ setDelegate:mockDelegate_];
    base::scoped_nsobject<TestWebViewContentView> webViewContentView(
        [[TestWebViewContentView alloc] initWithMockWebView:mockWebView_
                                                 scrollView:mockScrollView_]);
    [webController_ injectWebViewContentView:webViewContentView];

    NavigationManagerImpl& navigationManager =
        [webController_ webStateImpl]->GetNavigationManagerImpl();
    navigationManager.InitializeSession(@"name", nil, NO, 0);
    [navigationManager.GetSessionController()
          addPendingEntry:GURL("http://www.google.com/?q=foo#bar")
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_TYPED
        rendererInitiated:NO];
    // Set up child CRWWebController.
    mockChildWebController_.reset([[OCMockObject
        mockForProtocol:@protocol(CRWWebControllerScripting)] retain]);
    [[[mockDelegate_ stub] andReturn:mockChildWebController_.get()]
                           webController:webController_
        scriptingInterfaceForWindowNamed:@"http://www.google.com/#newtab"];
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mockDelegate_);
    EXPECT_OCMOCK_VERIFY(mockChildWebController_.get());
    EXPECT_OCMOCK_VERIFY(mockWebView_);
    [webController_ resetInjectedWebViewContentView];
    [webController_ setDelegate:nil];
    web::WebTestWithWKWebViewWebController::TearDown();
  }

  // The value for web view OCMock objects to expect for |-setFrame:|.
  CGRect ExpectedWebViewFrame() const {
    CGSize containerViewSize = [UIScreen mainScreen].bounds.size;
    containerViewSize.height -=
        CGRectGetHeight([UIApplication sharedApplication].statusBarFrame);
    return {CGPointZero, containerViewSize};
  }

  // Creates WebView mock.
  UIView* CreateMockWebView() const {
    id result = [[OCMockObject mockForClass:[WKWebView class]] retain];

    // Called by resetInjectedWebView
    [[result stub] backForwardList];
    [[[result stub] andReturn:[NSURL URLWithString:kTestURLString]] URL];
    [[result stub] setNavigationDelegate:OCMOCK_ANY];
    [[result stub] setUIDelegate:OCMOCK_ANY];
    [[result stub] setFrame:ExpectedWebViewFrame()];
    [[result stub] addObserver:webController_
                    forKeyPath:OCMOCK_ANY
                       options:0
                       context:nullptr];
    [[result stub] addObserver:OCMOCK_ANY
                    forKeyPath:@"scrollView.backgroundColor"
                       options:0
                       context:nullptr];

    [[result stub] removeObserver:webController_ forKeyPath:OCMOCK_ANY];
    [[result stub] removeObserver:OCMOCK_ANY
                       forKeyPath:@"scrollView.backgroundColor"];

    return result;
  }

  base::scoped_nsobject<UIScrollView> mockScrollView_;
  base::scoped_nsobject<id> mockWebView_;
  base::scoped_nsobject<id> mockDelegate_;
  base::scoped_nsobject<id> mockChildWebController_;
};

#define MAKE_URL(url_string) GURL([url_string UTF8String])

TEST_F(CRWWebControllerTest, UrlForHistoryNavigation) {
  NSArray* urlsNoFragments = @[
    @"http://one.com",
    @"http://two.com/",
    @"http://three.com/bar",
    @"http://four.com/bar/",
    @"five",
    @"/six",
    @"/seven/",
    @""
  ];

  NSArray* fragments = @[ @"#", @"#bar" ];
  NSMutableArray* urlsWithFragments = [NSMutableArray array];
  for (NSString* url in urlsNoFragments) {
    for (NSString* fragment in fragments) {
      [urlsWithFragments addObject:[url stringByAppendingString:fragment]];
    }
  }
  web::NavigationItemImpl fromItem;
  web::NavigationItemImpl toItem;

  // No start fragment: the end url is never changed.
  for (NSString* start in urlsNoFragments) {
    for (NSString* end in urlsWithFragments) {
      fromItem.SetURL(MAKE_URL(start));
      toItem.SetURL(MAKE_URL(end));
      EXPECT_EQ(MAKE_URL(end),
                [webController_ URLForHistoryNavigationFromItem:&fromItem
                                                         toItem:&toItem]);
    }
  }
  // Both contain fragments: the end url is never changed.
  for (NSString* start in urlsWithFragments) {
    for (NSString* end in urlsWithFragments) {
      fromItem.SetURL(MAKE_URL(start));
      toItem.SetURL(MAKE_URL(end));
      EXPECT_EQ(MAKE_URL(end),
                [webController_ URLForHistoryNavigationFromItem:&fromItem
                                                         toItem:&toItem]);
    }
  }
  for (unsigned start_index = 0; start_index < [urlsWithFragments count];
       ++start_index) {
    NSString* start = urlsWithFragments[start_index];
    for (unsigned end_index = 0; end_index < [urlsNoFragments count];
         ++end_index) {
      NSString* end = urlsNoFragments[end_index];
      if (start_index / 2 != end_index) {
        // The URLs have nothing in common, they are left untouched.
        fromItem.SetURL(MAKE_URL(start));
        toItem.SetURL(MAKE_URL(end));
        EXPECT_EQ(MAKE_URL(end),
                  [webController_ URLForHistoryNavigationFromItem:&fromItem
                                                           toItem:&toItem]);
      } else {
        // Start contains a fragment and matches end: An empty fragment is
        // added.
        fromItem.SetURL(MAKE_URL(start));
        toItem.SetURL(MAKE_URL(end));
        EXPECT_EQ(MAKE_URL([end stringByAppendingString:@"#"]),
                  [webController_ URLForHistoryNavigationFromItem:&fromItem
                                                           toItem:&toItem]);
      }
    }
  }
}

// Tests that presentSSLError:forSSLStatus:recoverable:callback: is called with
// correct arguments if WKWebView fails to load a page with bad SSL cert.
TEST_F(CRWWebControllerTest, SslCertError) {
  ASSERT_FALSE([mockDelegate_ SSLInfo].is_valid());

  scoped_refptr<net::X509Certificate> cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");

  NSArray* chain = @[ static_cast<id>(cert->os_cert_handle()) ];
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:@{
                        web::kNSErrorPeerCertificateChainKey : chain,
                      }];
  WKWebView* webView = static_cast<WKWebView*>([webController_ webView]);
  base::scoped_nsobject<NSObject> navigation([[NSObject alloc] init]);
  [static_cast<id<WKNavigationDelegate>>(webController_.get())
                            webView:webView
      didStartProvisionalNavigation:static_cast<WKNavigation*>(navigation)];
  [static_cast<id<WKNavigationDelegate>>(webController_.get())
                           webView:webView
      didFailProvisionalNavigation:static_cast<WKNavigation*>(navigation)
                         withError:error];

  // Verify correctness of delegate's method arguments.
  EXPECT_TRUE([mockDelegate_ SSLInfo].is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID, [mockDelegate_ SSLInfo].cert_status);
  EXPECT_EQ(net::CERT_STATUS_INVALID, [mockDelegate_ SSLStatus].cert_status);
  EXPECT_EQ(web::SECURITY_STYLE_AUTHENTICATION_BROKEN,
            [mockDelegate_ SSLStatus].security_style);
  EXPECT_FALSE([mockDelegate_ recoverable]);
  EXPECT_TRUE([mockDelegate_ shouldContinueCallback]);
}

// None of the |CRWWKWebViewWebControllerTest| setup is needed;
typedef web::WebTestWithWKWebViewWebController
    CRWWebControllerPageDialogsOpenPolicyTest;

TEST_F(CRWWebControllerPageDialogsOpenPolicyTest, SuppressPolicy) {
  LoadHtml(@"<html><body></body></html>");
  id delegate = [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
  [[delegate expect] webControllerDidSuppressDialog:webController_];

  [webController_ setDelegate:delegate];
  [webController_ setPageDialogOpenPolicy:web::DIALOG_POLICY_SUPPRESS];
  RunJavaScript(@"alert('')");

  WaitForBackgroundTasks();
  EXPECT_OCMOCK_VERIFY(delegate);
  [webController_ setDelegate:nil];
};

// A separate test class, as none of the |CRWWebControllerTest| setup is
// needed.
class CRWWebControllerPageScrollStateTest
    : public web::WebTestWithWKWebViewWebController {
 protected:
  // Returns a web::PageDisplayState that will scroll a WKWebView to
  // |scrollOffset| and zoom the content by |relativeZoomScale|.
  inline web::PageDisplayState CreateTestPageDisplayState(
      CGPoint scroll_offset,
      CGFloat relative_zoom_scale,
      CGFloat original_minimum_zoom_scale,
      CGFloat original_maximum_zoom_scale,
      CGFloat original_zoom_scale) const {
    return web::PageDisplayState(
        scroll_offset.x, scroll_offset.y, original_minimum_zoom_scale,
        original_maximum_zoom_scale,
        relative_zoom_scale * original_minimum_zoom_scale);
  }
};

// TODO(iOS): Flaky on the bots. crbug/493427
TEST_F(CRWWebControllerPageScrollStateTest,
       FLAKY_SetPageDisplayStateWithUserScalableDisabled) {
#if !TARGET_IPHONE_SIMULATOR
  // TODO(crbug.com/453530): fails flakily on device, so skip it there.
  return;
#endif
  web::PageZoomState zoom_state(1.0, 5.0, 1.0);
  LoadHtml(GetHTMLForZoomState(zoom_state, PAGE_SCALABILITY_DISABLED));
  CRWWebController* web_controller = webController_.get();
  WaitForZoomRendering(web_controller, zoom_state);
  web::PageZoomState original_zoom_state =
      web_controller.pageDisplayState.zoom_state();

  web::NavigationManager* nagivation_manager =
      web_controller.webState->GetNavigationManager();
  nagivation_manager->GetLastCommittedItem()->SetPageDisplayState(
      CreateTestPageDisplayState(CGPointMake(1.0, 1.0),  // scroll offset
                                 3.0,                    // relative zoom scale
                                 1.0,    // original minimum zoom scale
                                 5.0,    // original maximum zoom scale
                                 1.0));  // original zoom scale
  [web_controller restoreStateFromHistory];

  // |-restoreStateFromHistory| is async; wait for its completion.
  base::test::ios::WaitUntilCondition(^bool() {
    return web_controller.pageDisplayState.scroll_state().offset_x() == 1.0;
  });

  ASSERT_EQ(original_zoom_state, web_controller.pageDisplayState.zoom_state());
};

// TODO(iOS): Flaky on the bots. crbug/493427
TEST_F(CRWWebControllerPageScrollStateTest,
       FLAKY_SetPageDisplayStateWithUserScalableEnabled) {
  web::PageZoomState zoom_state(1.0, 10.0, 1.0);
  LoadHtml(GetHTMLForZoomState(zoom_state, PAGE_SCALABILITY_ENABLED));
  CRWWebController* web_controller = webController_.get();
  WaitForZoomRendering(web_controller, zoom_state);

  web::NavigationManager* nagivation_manager =
      web_controller.webState->GetNavigationManager();
  nagivation_manager->GetLastCommittedItem()->SetPageDisplayState(
      CreateTestPageDisplayState(CGPointMake(1.0, 1.0),  // scroll offset
                                 3.0,                    // relative zoom scale
                                 1.0,    // original minimum zoom scale
                                 10.0,   // original maximum zoom scale
                                 1.0));  // original zoom scale
  [web_controller restoreStateFromHistory];

  // |-restoreStateFromHistory| is async; wait for its completion.
  base::test::ios::WaitUntilCondition(^bool() {
    return web_controller.pageDisplayState.scroll_state().offset_x() == 1.0;
  });

  web::PageZoomState final_zoom_state =
      web_controller.pageDisplayState.zoom_state();
  EXPECT_FLOAT_EQ(3, final_zoom_state.zoom_scale() /
                        final_zoom_state.minimum_zoom_scale());
};

// TODO(iOS): Flaky on the bots. crbug/493427
TEST_F(CRWWebControllerPageScrollStateTest, FLAKY_AtTop) {
  // This test fails on iPhone 6/6+; skip until it's fixed. crbug.com/453105
  if (IsIPhone6Or6Plus())
    return;

  web::PageZoomState zoom_state = web::PageZoomState(1.0, 5.0, 1.0);
  LoadHtml(GetHTMLForZoomState(zoom_state, PAGE_SCALABILITY_ENABLED));
  CRWWebController* web_controller = webController_.get();
  WaitForZoomRendering(web_controller, zoom_state);
  ASSERT_TRUE(web_controller.atTop);

  web::NavigationManager* nagivation_manager =
      web_controller.webState->GetNavigationManager();
  nagivation_manager->GetLastCommittedItem()->SetPageDisplayState(
      CreateTestPageDisplayState(CGPointMake(0.0, 30.0),  // scroll offset
                                 5.0,                     // relative zoom scale
                                 1.0,    // original minimum zoom scale
                                 5.0,    // original maximum zoom scale
                                 1.0));  // original zoom scale
  [web_controller restoreStateFromHistory];

  // |-restoreStateFromHistory| is async; wait for its completion.
  base::test::ios::WaitUntilCondition(^bool() {
    return web_controller.pageDisplayState.scroll_state().offset_y() == 30.0;
  });

  ASSERT_FALSE(web_controller.atTop);
};

// Real WKWebView is required for JSEvaluationTest.
typedef web::WebTestWithWKWebViewWebController CRWWebControllerJSEvaluationTest;

// Tests that a script correctly evaluates to string.
TEST_F(CRWWebControllerJSEvaluationTest, Evaluation) {
  LoadHtml(@"<p></p>");
  EXPECT_NSEQ(@"true", EvaluateJavaScriptAsString(@"true"));
  EXPECT_NSEQ(@"false", EvaluateJavaScriptAsString(@"false"));
}

// Tests that a script is not evaluated on windowID mismatch.
TEST_F(CRWWebControllerJSEvaluationTest, WindowIdMissmatch) {
  LoadHtml(@"<p></p>");
  // Script is evaluated since windowID is matched.
  EvaluateJavaScriptAsString(@"window.test1 = '1';");
  EXPECT_NSEQ(@"1", EvaluateJavaScriptAsString(@"window.test1"));

  // Change windowID.
  EvaluateJavaScriptAsString(@"__gCrWeb['windowId'] = '';");

  // Script is not evaluated because of windowID mismatch.
  EvaluateJavaScriptAsString(@"window.test2 = '2';");
  EXPECT_NSEQ(@"", EvaluateJavaScriptAsString(@"window.test2"));
}

TEST_F(CRWWebControllerTest, WebUrlWithTrustLevel) {
  [[[mockWebView_ stub] andReturn:[NSURL URLWithString:kTestURLString]] URL];
  [[[mockWebView_ stub] andReturnBool:NO] hasOnlySecureContent];

  // Stub out the injection process.
  [[mockWebView_ stub] evaluateJavaScript:OCMOCK_ANY
                        completionHandler:OCMOCK_ANY];

  // Simulate registering load request to avoid failing page load simulation.
  [webController_ simulateLoadRequestWithURL:GURL([kTestURLString UTF8String])];
  // Simulate a page load to trigger a URL update.
  [static_cast<id<WKNavigationDelegate>>(webController_.get())
                  webView:mockWebView_
      didCommitNavigation:nil];

  web::URLVerificationTrustLevel trust_level = web::kNone;
  GURL gurl = [webController_ currentURLWithTrustLevel:&trust_level];

  EXPECT_EQ(gurl, GURL(base::SysNSStringToUTF8(kTestURLString)));
  EXPECT_EQ(web::kAbsolute, trust_level);
}

// A separate test class, as none of the |CRWUIWebViewWebControllerTest| setup
// is needed;
typedef web::WebTestWithWKWebViewWebController CRWWebControllerObserversTest;

// Tests that CRWWebControllerObservers are called.
TEST_F(CRWWebControllerObserversTest, Observers) {
  base::scoped_nsobject<CountingObserver> observer(
      [[CountingObserver alloc] init]);
  CRWWebController* web_controller = webController_;
  EXPECT_EQ(0u, [web_controller observerCount]);
  [web_controller addObserver:observer];
  EXPECT_EQ(1u, [web_controller observerCount]);

  EXPECT_EQ(0, [observer pageLoadedCount]);
  [web_controller webStateImpl]->OnPageLoaded(GURL("http://test"), false);
  EXPECT_EQ(0, [observer pageLoadedCount]);
  [web_controller webStateImpl]->OnPageLoaded(GURL("http://test"), true);
  EXPECT_EQ(1, [observer pageLoadedCount]);

  EXPECT_EQ(0, [observer messageCount]);
  // Non-matching prefix.
  EXPECT_FALSE([web_controller webStateImpl]->OnScriptCommandReceived(
      "a", base::DictionaryValue(), GURL("http://test"), true));
  EXPECT_EQ(0, [observer messageCount]);
  // Matching prefix.
  EXPECT_TRUE([web_controller webStateImpl]->OnScriptCommandReceived(
      base::SysNSStringToUTF8([observer commandPrefix]) + ".foo",
      base::DictionaryValue(), GURL("http://test"), true));
  EXPECT_EQ(1, [observer messageCount]);

  [web_controller removeObserver:observer];
  EXPECT_EQ(0u, [web_controller observerCount]);
};

// Test fixture for window.open tests.
class CRWWebControllerWindowOpenTest
    : public web::WebTestWithWKWebViewWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWKWebViewWebController::SetUp();

    // Configure web delegate.
    delegate_.reset([[MockInteractionLoader alloc]
        initWithRepresentedObject:
            [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)]]);
    ASSERT_TRUE([delegate_ blockPopups]);
    [webController_ setDelegate:delegate_];

    // Configure child web controller.
    child_.reset(CreateWebController());
    [child_ setWebUsageEnabled:YES];
    [delegate_ setChildWebController:child_];

    // Configure child web controller's session controller mock.
    id sessionController =
        [OCMockObject niceMockForClass:[CRWSessionController class]];
    BOOL yes = YES;
    [[[sessionController stub] andReturnValue:OCMOCK_VALUE(yes)] isOpenedByDOM];
    [child_ webStateImpl]->GetNavigationManagerImpl().SetSessionController(
        sessionController);

    LoadHtml(@"<html><body></body></html>");
  }
  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
    [webController_ setDelegate:nil];
    [child_ close];

    web::WebTestWithWKWebViewWebController::TearDown();
  }
  // Executes JavaScript that opens a new window and returns evaluation result
  // as a string.
  NSString* OpenWindowByDOM() {
    NSString* const kOpenWindowScript =
        @"var w = window.open('javascript:void(0);', target='_blank');"
         "w.toString();";
    NSString* windowJSObject = EvaluateJavaScriptAsString(kOpenWindowScript);
    WaitForBackgroundTasks();
    return windowJSObject;
  }
  // A CRWWebDelegate mock used for testing.
  base::scoped_nsobject<id> delegate_;
  // A child CRWWebController used for testing.
  base::scoped_nsobject<CRWWebController> child_;
};

// Tests that absence of web delegate is handled gracefully.
TEST_F(CRWWebControllerWindowOpenTest, NoDelegate) {
  [webController_ setDelegate:nil];

  EXPECT_NSEQ(@"", OpenWindowByDOM());

  EXPECT_FALSE([delegate_ blockedPopupInfo]);
}

// Tests that window.open triggered by user gesture opens a new non-popup
// window.
TEST_F(CRWWebControllerWindowOpenTest, OpenWithUserGesture) {
  SEL selector = @selector(webPageOrderedOpen);
  [delegate_ onSelector:selector
      callBlockExpectation:^(){
      }];

  [webController_ touched:YES];
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDOM());
  EXPECT_FALSE([delegate_ blockedPopupInfo]);
}

// Tests that window.open executed w/o user gesture does not open a new window.
// Once the blocked popup is allowed a new window is opened.
TEST_F(CRWWebControllerWindowOpenTest, AllowPopup) {
  SEL selector =
      @selector(webPageOrderedOpen:referrer:windowName:inBackground:);
  [delegate_ onSelector:selector
      callBlockExpectation:^(const GURL& new_window_url,
                             const web::Referrer& referrer,
                             NSString* obsoleted_window_name,
                             BOOL in_background) {
        EXPECT_EQ("javascript:void(0);", new_window_url.spec());
        EXPECT_EQ("", referrer.url.spec());
        EXPECT_FALSE(in_background);
      }];

  ASSERT_FALSE([webController_ userIsInteracting]);
  EXPECT_NSEQ(@"", OpenWindowByDOM());
  base::test::ios::WaitUntilCondition(^bool() {
    return [delegate_ blockedPopupInfo];
  });

  if ([delegate_ blockedPopupInfo])
    [delegate_ blockedPopupInfo]->ShowPopup();

  EXPECT_EQ("", [delegate_ sourceURL].spec());
  EXPECT_EQ("javascript:void(0);", [delegate_ popupURL].spec());
}

// Tests that window.open executed w/o user gesture opens a new window, assuming
// that delegate allows popups.
TEST_F(CRWWebControllerWindowOpenTest, DontBlockPopup) {
  [delegate_ setBlockPopups:NO];
  SEL selector = @selector(webPageOrderedOpen);
  [delegate_ onSelector:selector
      callBlockExpectation:^(){
      }];

  EXPECT_NSEQ(@"[object Window]", OpenWindowByDOM());
  EXPECT_FALSE([delegate_ blockedPopupInfo]);

  EXPECT_EQ("", [delegate_ sourceURL].spec());
  EXPECT_EQ("javascript:void(0);", [delegate_ popupURL].spec());
}

// Tests that window.open executed w/o user gesture does not open a new window.
TEST_F(CRWWebControllerWindowOpenTest, BlockPopup) {
  ASSERT_FALSE([webController_ userIsInteracting]);
  EXPECT_NSEQ(@"", OpenWindowByDOM());
  base::test::ios::WaitUntilCondition(^bool() {
    return [delegate_ blockedPopupInfo];
  });

  EXPECT_EQ("", [delegate_ sourceURL].spec());
  EXPECT_EQ("javascript:void(0);", [delegate_ popupURL].spec());
};

// Fixture class to test WKWebView crashes.
class CRWWebControllerWebProcessTest
    : public web::WebTestWithWKWebViewWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWKWebViewWebController::SetUp();
    webView_.reset(web::CreateTerminatedWKWebView());
    base::scoped_nsobject<TestWebViewContentView> webViewContentView(
        [[TestWebViewContentView alloc]
            initWithMockWebView:webView_
                     scrollView:[webView_ scrollView]]);
    [webController_ injectWebViewContentView:webViewContentView];
  }
  base::scoped_nsobject<WKWebView> webView_;
};

// Tests that -[CRWWebDelegate webControllerWebProcessDidCrash:] is called
// when WKWebView web process has crashed.
TEST_F(CRWWebControllerWebProcessTest, Crash) {
  id delegate = [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
  [[delegate expect] webControllerWebProcessDidCrash:webController_];

  [webController_ setDelegate:delegate];
  web::SimulateWKWebViewCrash(webView_);

  EXPECT_OCMOCK_VERIFY(delegate);
  EXPECT_FALSE([webController_ isViewAlive]);
  [webController_ setDelegate:nil];
};

}  // namespace
