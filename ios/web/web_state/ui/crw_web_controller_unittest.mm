// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <WebKit/WebKit.h>

#include <utility>

#include "base/ios/ios_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/values.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#include "ios/web/navigation/crw_session_controller.h"
#include "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/test/test_native_content.h"
#import "ios/web/public/test/test_native_content_provider.h"
#import "ios/web/public/test/test_web_client.h"
#include "ios/web/public/test/test_web_view_content_view.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "ios/web/test/wk_web_view_crash_utils.h"
#include "ios/web/web_state/blocked_popup_info.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"
#include "ui/base/test/ios/ui_view_test_utils.h"

using web::NavigationManagerImpl;

@interface CRWWebController (PrivateAPI)
@property(nonatomic, readwrite) web::PageDisplayState pageDisplayState;
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
@end

// Stub implementation for CRWWebUserInterfaceDelegate protocol.
@interface CRWWebUserInterfaceDelegateStub
    : OCMockComplexTypeHelper<CRWWebUserInterfaceDelegate>
@end

@implementation CRWWebUserInterfaceDelegateStub

- (void)webController:(CRWWebController*)webController
    runJavaScriptAlertPanelWithMessage:(NSString*)message
                            requestURL:(const GURL&)requestURL
                     completionHandler:(void (^)(void))completionHandler {
  void (^stubBlock)(CRWWebController*, NSString*, const GURL&, id) =
      [self blockForSelector:_cmd];
  stubBlock(webController, message, requestURL, completionHandler);
}

- (void)webController:(CRWWebController*)webController
    runJavaScriptConfirmPanelWithMessage:(NSString*)message
                              requestURL:(const GURL&)requestURL
                       completionHandler:(void (^)(BOOL))completionHandler {
  void (^stubBlock)(CRWWebController*, NSString*, const GURL&, id) =
      [self blockForSelector:_cmd];
  stubBlock(webController, message, requestURL, completionHandler);
}

- (void)webController:(CRWWebController*)webController
    runJavaScriptTextInputPanelWithPrompt:(NSString*)message
                              defaultText:(NSString*)defaultText
                               requestURL:(const GURL&)requestURL
                        completionHandler:
                            (void (^)(NSString* input))completionHandler {
  void (^stubBlock)(CRWWebController*, NSString*, NSString*, const GURL&, id) =
      [self blockForSelector:_cmd];
  stubBlock(webController, message, defaultText, requestURL, completionHandler);
}

- (BOOL)respondsToSelector:(SEL)selector {
  // OCMockComplexTypeHelper DCHECKs when respondsToSelector: is called for
  // expected selector.
  if (selector == @selector(webController:
                      runJavaScriptAlertPanelWithMessage:
                                              requestURL:
                                       completionHandler:)) {
    return YES;
  }
  if (selector == @selector(webController:
                      runJavaScriptConfirmPanelWithMessage:
                                                requestURL:
                                         completionHandler:)) {
    return YES;
  }
  if (selector == @selector(webController:
                      runJavaScriptTextInputPanelWithPrompt:
                                                defaultText:
                                                 requestURL:
                                          completionHandler:)) {
    return YES;
  }
  return [super respondsToSelector:selector];
}
@end

@implementation MockInteractionLoader {
  // Backs up the property with the same name.
  std::unique_ptr<web::BlockedPopupInfo> _blockedPopupInfo;
}
@synthesize popupURL = _popupURL;
@synthesize sourceURL = _sourceURL;
@synthesize blockPopups = _blockPopups;
@synthesize childWebController = _childWebController;

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
- (BOOL)webController:(CRWWebController*)webController
        shouldOpenURL:(const GURL&)URL
      mainDocumentURL:(const GURL&)mainDocumentURL
          linkClicked:(BOOL)linkClicked {
  return YES;
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

const char kTestURLString[] = "http://www.google.com/";
const char kTestAppSpecificURL[] = "testwebui://test/";

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
class CRWWebControllerTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    mockWebView_.reset(CreateMockWebView());
    mockScrollView_.reset([[UIScrollView alloc] init]);
    [[[mockWebView_ stub] andReturn:mockScrollView_.get()] scrollView];

    id originalMockDelegate =
        [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
    mockDelegate_.reset([[MockInteractionLoader alloc]
        initWithRepresentedObject:originalMockDelegate]);
    [web_controller() setDelegate:mockDelegate_];
    base::scoped_nsobject<TestWebViewContentView> webViewContentView(
        [[TestWebViewContentView alloc] initWithMockWebView:mockWebView_
                                                 scrollView:mockScrollView_]);
    [web_controller() injectWebViewContentView:webViewContentView];

    NavigationManagerImpl& navigationManager =
        [web_controller() webStateImpl]->GetNavigationManagerImpl();
    navigationManager.InitializeSession(@"name", nil, NO, 0);
    [navigationManager.GetSessionController()
          addPendingEntry:GURL("http://www.google.com/?q=foo#bar")
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_TYPED
        rendererInitiated:NO];
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mockDelegate_);
    EXPECT_OCMOCK_VERIFY(mockChildWebController_);
    EXPECT_OCMOCK_VERIFY(mockWebView_);
    [web_controller() resetInjectedWebViewContentView];
    [web_controller() setDelegate:nil];
    web::WebTestWithWebController::TearDown();
  }

  // The value for web view OCMock objects to expect for |-setFrame:|.
  CGRect ExpectedWebViewFrame() const {
    CGSize containerViewSize = [UIScreen mainScreen].bounds.size;
    containerViewSize.height -=
        CGRectGetHeight([UIApplication sharedApplication].statusBarFrame);
    return {CGPointZero, containerViewSize};
  }

  // Creates WebView mock.
  UIView* CreateMockWebView() {
    id result = [[OCMockObject mockForClass:[WKWebView class]] retain];

    if (base::ios::IsRunningOnIOS10OrLater()) {
      [[result stub] serverTrust];
    } else {
      [[result stub] certificateChain];
    }

    [[result stub] backForwardList];
    [[[result stub] andReturn:[NSURL URLWithString:@(kTestURLString)]] URL];
    [[result stub] setNavigationDelegate:OCMOCK_ANY];
    [[result stub] setUIDelegate:OCMOCK_ANY];
    [[result stub] setFrame:ExpectedWebViewFrame()];
    [[result stub] addObserver:web_controller()
                    forKeyPath:OCMOCK_ANY
                       options:0
                       context:nullptr];
    [[result stub] addObserver:OCMOCK_ANY
                    forKeyPath:@"scrollView.backgroundColor"
                       options:0
                       context:nullptr];

    [[result stub] removeObserver:web_controller() forKeyPath:OCMOCK_ANY];
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
                [web_controller() URLForHistoryNavigationFromItem:&fromItem
                                                           toItem:&toItem]);
    }
  }
  // Both contain fragments: the end url is never changed.
  for (NSString* start in urlsWithFragments) {
    for (NSString* end in urlsWithFragments) {
      fromItem.SetURL(MAKE_URL(start));
      toItem.SetURL(MAKE_URL(end));
      EXPECT_EQ(MAKE_URL(end),
                [web_controller() URLForHistoryNavigationFromItem:&fromItem
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
                  [web_controller() URLForHistoryNavigationFromItem:&fromItem
                                                             toItem:&toItem]);
      } else {
        // Start contains a fragment and matches end: An empty fragment is
        // added.
        fromItem.SetURL(MAKE_URL(start));
        toItem.SetURL(MAKE_URL(end));
        EXPECT_EQ(MAKE_URL([end stringByAppendingString:@"#"]),
                  [web_controller() URLForHistoryNavigationFromItem:&fromItem
                                                             toItem:&toItem]);
      }
    }
  }
}

// Tests that AllowCertificateError is called with correct arguments if
// WKWebView fails to load a page with bad SSL cert.
TEST_F(CRWWebControllerTest, SslCertError) {
  // Last arguments passed to AllowCertificateError must be in default state.
  ASSERT_FALSE(GetWebClient()->last_cert_error_code());
  ASSERT_FALSE(GetWebClient()->last_cert_error_ssl_info().is_valid());
  ASSERT_FALSE(GetWebClient()->last_cert_error_ssl_info().cert_status);
  ASSERT_FALSE(GetWebClient()->last_cert_error_request_url().is_valid());
  ASSERT_TRUE(GetWebClient()->last_cert_error_overridable());

  scoped_refptr<net::X509Certificate> cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");

  NSArray* chain = @[ static_cast<id>(cert->os_cert_handle()) ];
  GURL url("https://chromium.test");
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:@{
                        web::kNSErrorPeerCertificateChainKey : chain,
                        web::kNSErrorFailingURLKey : net::NSURLWithGURL(url),
                      }];
  CRWWebControllerContainerView* containerView =
      static_cast<CRWWebControllerContainerView*>([web_controller() view]);
  WKWebView* webView =
      static_cast<WKWebView*>(containerView.webViewContentView.webView);
  base::scoped_nsobject<NSObject> navigation([[NSObject alloc] init]);
  [static_cast<id<WKNavigationDelegate>>(web_controller())
                            webView:webView
      didStartProvisionalNavigation:static_cast<WKNavigation*>(navigation)];
  [static_cast<id<WKNavigationDelegate>>(web_controller())
                           webView:webView
      didFailProvisionalNavigation:static_cast<WKNavigation*>(navigation)
                         withError:error];

  // Verify correctness of AllowCertificateError method call.
  EXPECT_EQ(net::ERR_CERT_INVALID, GetWebClient()->last_cert_error_code());
  EXPECT_TRUE(GetWebClient()->last_cert_error_ssl_info().is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID,
            GetWebClient()->last_cert_error_ssl_info().cert_status);
  EXPECT_EQ(url, GetWebClient()->last_cert_error_request_url());
  EXPECT_FALSE(GetWebClient()->last_cert_error_overridable());
}

// Test fixture to test |setPageDialogOpenPolicy:|.
class CRWWebControllerPageDialogOpenPolicyTest
    : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    LoadHtml(@"<html><body></body></html>");
    web_delegate_mock_.reset(
        [[OCMockObject mockForProtocol:@protocol(CRWWebDelegate)] retain]);
    [web_controller() setDelegate:web_delegate_mock_];
    id ui_delegate_oc_mock =
        [OCMockObject mockForProtocol:@protocol(CRWWebUserInterfaceDelegate)];
    ui_delegate_mock_.reset([[CRWWebUserInterfaceDelegateStub alloc]
        initWithRepresentedObject:ui_delegate_oc_mock]);
    [web_controller() setUIDelegate:ui_delegate_mock_];
    // Web Controller cancels all dialogs on |close|.
    [[ui_delegate_mock_ stub] cancelDialogsForWebController:web_controller()];
  }
  void TearDown() override {
    WaitForBackgroundTasks();
    EXPECT_OCMOCK_VERIFY(web_delegate_mock_);
    EXPECT_OCMOCK_VERIFY(ui_delegate_mock_);
    [web_controller() setDelegate:nil];
    [web_controller() setUIDelegate:nil];

    web::WebTestWithWebController::TearDown();
  }
  // Returns CRWWebDelegate mock object.
  id web_delegate_mock() { return web_delegate_mock_; };
  // Returns CRWWebUserInterfaceDelegate mock object.
  id ui_delegate_mock() { return ui_delegate_mock_; };

 private:
  // Mocks CRWWebDelegate object.
  base::scoped_nsprotocol<id> web_delegate_mock_;
  // Mocks CRWWebUserInterfaceDelegate object.
  base::scoped_nsprotocol<id> ui_delegate_mock_;
};

// Tests that window.alert dialog is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressAlert) {
  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  ExecuteJavaScript(@"alert('test')");
};

// Tests that window.alert dialog is shown for DIALOG_POLICY_ALLOW.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, AllowAlert) {
  SEL selector = @selector(webController:
      runJavaScriptAlertPanelWithMessage:
                              requestURL:
                       completionHandler:);
  [ui_delegate_mock() onSelector:selector
            callBlockExpectation:^(CRWWebController* controller,
                                   NSString* message, const GURL& url,
                                   ProceduralBlock completion_handler) {
              EXPECT_NSEQ(web_controller(), controller);
              EXPECT_NSEQ(@"test", message);
              web::URLVerificationTrustLevel unused;
              EXPECT_EQ([controller currentURLWithTrustLevel:&unused], url);
              completion_handler();
            }];

  [web_controller() setShouldSuppressDialogs:NO];
  ExecuteJavaScript(@"alert('test')");
};

// Tests that window.confirm dialog is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressConfirm) {
  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"confirm('test')"));
};

// Tests that window.confirm dialog is shown for DIALOG_POLICY_ALLOW and
// it's result is true.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, AllowConfirmWithTrue) {
  SEL selector = @selector(webController:
      runJavaScriptConfirmPanelWithMessage:
                                requestURL:
                         completionHandler:);
  [ui_delegate_mock()
                onSelector:selector
      callBlockExpectation:^(CRWWebController* controller, NSString* message,
                             const GURL& url, id completion_handler) {
        EXPECT_NSEQ(web_controller(), controller);
        EXPECT_NSEQ(@"test", message);
        web::URLVerificationTrustLevel unused;
        EXPECT_EQ([controller currentURLWithTrustLevel:&unused], url);
        void (^callable_block)(BOOL) = completion_handler;
        callable_block(YES);
      }];

  [web_controller() setShouldSuppressDialogs:NO];
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"confirm('test')"));
}

// Tests that window.confirm dialog is shown for DIALOG_POLICY_ALLOW and
// it's result is false.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, AllowConfirmWithFalse) {
  SEL selector = @selector(webController:
      runJavaScriptConfirmPanelWithMessage:
                                requestURL:
                         completionHandler:);
  [ui_delegate_mock()
                onSelector:selector
      callBlockExpectation:^(CRWWebController* controller, NSString* message,
                             const GURL& url, id completion_handler) {
        EXPECT_NSEQ(web_controller(), controller);
        EXPECT_NSEQ(@"test", message);
        web::URLVerificationTrustLevel unused;
        EXPECT_EQ([controller currentURLWithTrustLevel:&unused], url);
        void (^callable_block)(BOOL) = completion_handler;
        callable_block(NO);
      }];

  [web_controller() setShouldSuppressDialogs:NO];
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"confirm('test')"));
}

// Tests that window.prompt dialog is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressPrompt) {
  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  EXPECT_EQ([NSNull null], ExecuteJavaScript(@"prompt('Yes?', 'No')"));
}

// Tests that window.prompt dialog is shown for DIALOG_POLICY_ALLOW.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, AllowPrompt) {
  SEL selector = @selector(webController:
      runJavaScriptTextInputPanelWithPrompt:
                                defaultText:
                                 requestURL:
                          completionHandler:);
  [ui_delegate_mock() onSelector:selector
            callBlockExpectation:^(CRWWebController* controller,
                                   NSString* message, NSString* default_text,
                                   const GURL& url, id completion_handler) {
              EXPECT_NSEQ(web_controller(), controller);
              EXPECT_NSEQ(@"Yes?", message);
              EXPECT_NSEQ(@"No", default_text);
              web::URLVerificationTrustLevel unused;
              EXPECT_EQ([controller currentURLWithTrustLevel:&unused], url);
              void (^callable_block)(NSString*) = completion_handler;
              callable_block(@"Maybe");
            }];

  [web_controller() setShouldSuppressDialogs:NO];
  EXPECT_NSEQ(@"Maybe", ExecuteJavaScript(@"prompt('Yes?', 'No')"));
}

// Tests that geolocation dialog is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressGeolocation) {
  // The geolocation APIs require HTTPS on iOS 10, which can not be simulated
  // even using |loadHTMLString:baseURL:| WKWebView API.
  if (base::ios::IsRunningOnIOS10OrLater()) {
    return;
  }

  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  ExecuteJavaScript(@"navigator.geolocation.getCurrentPosition()");
}

// Tests that window.open is suppressed for DIALOG_POLICY_SUPPRESS.
TEST_F(CRWWebControllerPageDialogOpenPolicyTest, SuppressWindowOpen) {
  [[web_delegate_mock() expect]
      webControllerDidSuppressDialog:web_controller()];
  [web_controller() setShouldSuppressDialogs:YES];
  ExecuteJavaScript(@"window.open('')");
}

// A separate test class, as none of the |CRWWebControllerTest| setup is
// needed.
class CRWWebControllerPageScrollStateTest
    : public web::WebTestWithWebController {
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
  WaitForZoomRendering(web_controller(), zoom_state);
  web::PageZoomState original_zoom_state =
      web_controller().pageDisplayState.zoom_state();

  web::NavigationManager* nagivation_manager =
      web_state()->GetNavigationManager();
  nagivation_manager->GetLastCommittedItem()->SetPageDisplayState(
      CreateTestPageDisplayState(CGPointMake(1.0, 1.0),  // scroll offset
                                 3.0,                    // relative zoom scale
                                 1.0,    // original minimum zoom scale
                                 5.0,    // original maximum zoom scale
                                 1.0));  // original zoom scale
  [web_controller() restoreStateFromHistory];

  // |-restoreStateFromHistory| is async; wait for its completion.
  base::test::ios::WaitUntilCondition(^bool() {
    return web_controller().pageDisplayState.scroll_state().offset_x() == 1.0;
  });

  ASSERT_EQ(original_zoom_state,
            web_controller().pageDisplayState.zoom_state());
};

// TODO(iOS): Flaky on the bots. crbug/493427
TEST_F(CRWWebControllerPageScrollStateTest,
       FLAKY_SetPageDisplayStateWithUserScalableEnabled) {
  web::PageZoomState zoom_state(1.0, 5.0, 1.0);

  LoadHtml(GetHTMLForZoomState(zoom_state, PAGE_SCALABILITY_ENABLED));
  WaitForZoomRendering(web_controller(), zoom_state);

  web::NavigationManager* nagivation_manager =
      web_state()->GetNavigationManager();
  nagivation_manager->GetLastCommittedItem()->SetPageDisplayState(
      CreateTestPageDisplayState(CGPointMake(1.0, 1.0),  // scroll offset
                                 3.0,                    // relative zoom scale
                                 1.0,    // original minimum zoom scale
                                 5.0,    // original maximum zoom scale
                                 1.0));  // original zoom scale
  [web_controller() restoreStateFromHistory];

  // |-restoreStateFromHistory| is async; wait for its completion.
  base::test::ios::WaitUntilCondition(^bool() {
    return web_controller().pageDisplayState.scroll_state().offset_x() == 1.0;
  });

  web::PageZoomState final_zoom_state =
      web_controller().pageDisplayState.zoom_state();
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
  WaitForZoomRendering(web_controller(), zoom_state);
  ASSERT_TRUE(web_controller().atTop);

  web::NavigationManager* nagivation_manager =
      web_state()->GetNavigationManager();
  nagivation_manager->GetLastCommittedItem()->SetPageDisplayState(
      CreateTestPageDisplayState(CGPointMake(0.0, 30.0),  // scroll offset
                                 5.0,                     // relative zoom scale
                                 1.0,    // original minimum zoom scale
                                 5.0,    // original maximum zoom scale
                                 1.0));  // original zoom scale
  [web_controller() restoreStateFromHistory];

  // |-restoreStateFromHistory| is async; wait for its completion.
  base::test::ios::WaitUntilCondition(^bool() {
    return web_controller().pageDisplayState.scroll_state().offset_y() == 30.0;
  });

  ASSERT_FALSE(web_controller().atTop);
};

// Real WKWebView is required for CRWWebControllerNavigationTest.
typedef web::WebTestWithWebController CRWWebControllerNavigationTest;

// Tests navigation between 2 URLs which differ only by fragment.
TEST_F(CRWWebControllerNavigationTest, GoToEntryWithoutDocumentChange) {
  LoadHtml(@"<html><body></body></html>", GURL("https://chromium.test"));
  LoadHtml(@"<html><body></body></html>", GURL("https://chromium.test#hash"));
  NavigationManagerImpl& nav_manager =
      web_controller().webStateImpl->GetNavigationManagerImpl();
  CRWSessionController* session_controller = nav_manager.GetSessionController();
  EXPECT_EQ(2U, session_controller.entries.count);
  EXPECT_NSEQ(session_controller.entries.lastObject,
              session_controller.currentEntry);

  [web_controller() goToItemAtIndex:0];
  EXPECT_NSEQ(session_controller.entries.firstObject,
              session_controller.currentEntry);
}

// Real WKWebView is required for CRWWebControllerJSExecutionTest.
typedef web::WebTestWithWebController CRWWebControllerJSExecutionTest;

// Tests that a script correctly evaluates to boolean.
TEST_F(CRWWebControllerJSExecutionTest, Execution) {
  LoadHtml(@"<p></p>");
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"true"));
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"false"));
}

// Tests that a script is not executed on windowID mismatch.
TEST_F(CRWWebControllerJSExecutionTest, WindowIdMissmatch) {
  LoadHtml(@"<p></p>");
  // Script is evaluated since windowID is matched.
  ExecuteJavaScript(@"window.test1 = '1';");
  EXPECT_NSEQ(@"1", ExecuteJavaScript(@"window.test1"));

  // Change windowID.
  ExecuteJavaScript(@"__gCrWeb['windowId'] = '';");

  // Script is not evaluated because of windowID mismatch.
  ExecuteJavaScript(@"window.test2 = '2';");
  EXPECT_FALSE(ExecuteJavaScript(@"window.test2"));
}

TEST_F(CRWWebControllerTest, WebUrlWithTrustLevel) {
  [[[mockWebView_ stub] andReturn:[NSURL URLWithString:@(kTestURLString)]] URL];
  [[[mockWebView_ stub] andReturnBool:NO] hasOnlySecureContent];

  // Stub out the injection process.
  [[mockWebView_ stub] evaluateJavaScript:OCMOCK_ANY
                        completionHandler:OCMOCK_ANY];

  // Simulate registering load request to avoid failing page load simulation.
  [web_controller() simulateLoadRequestWithURL:GURL(kTestURLString)];
  // Simulate a page load to trigger a URL update.
  [static_cast<id<WKNavigationDelegate>>(web_controller()) webView:mockWebView_
                                               didCommitNavigation:nil];

  web::URLVerificationTrustLevel trust_level = web::kNone;
  GURL gurl = [web_controller() currentURLWithTrustLevel:&trust_level];

  EXPECT_EQ(gurl, GURL(kTestURLString));
  EXPECT_EQ(web::kAbsolute, trust_level);
}

// Test fixture for testing CRWWebController presenting native content.
class CRWWebControllerNativeContentTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    mock_native_provider_.reset([[TestNativeContentProvider alloc] init]);
    [web_controller() setNativeProvider:mock_native_provider_];
  }

  void Load(const GURL& URL) {
    NavigationManagerImpl& navigation_manager =
        [web_controller() webStateImpl]->GetNavigationManagerImpl();
    navigation_manager.InitializeSession(@"name", nil, NO, 0);
    [navigation_manager.GetSessionController()
          addPendingEntry:URL
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_TYPED
        rendererInitiated:NO];
    [web_controller() loadCurrentURL];
  }

  base::scoped_nsobject<TestNativeContentProvider> mock_native_provider_;
};

// Tests WebState and NavigationManager correctly return native content URL.
TEST_F(CRWWebControllerNativeContentTest, NativeContentURL) {
  GURL url_to_load(kTestAppSpecificURL);
  base::scoped_nsobject<TestNativeContent> content(
      [[TestNativeContent alloc] initWithURL:url_to_load virtualURL:GURL()]);
  [mock_native_provider_ setController:content forURL:url_to_load];
  Load(url_to_load);
  web::URLVerificationTrustLevel trust_level = web::kNone;
  GURL gurl = [web_controller() currentURLWithTrustLevel:&trust_level];
  EXPECT_EQ(gurl, url_to_load);
  EXPECT_EQ(web::kAbsolute, trust_level);
  EXPECT_EQ([web_controller() webState]->GetVisibleURL(), url_to_load);
  NavigationManagerImpl& navigationManager =
      [web_controller() webStateImpl]->GetNavigationManagerImpl();
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetVirtualURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetVirtualURL(),
            url_to_load);
}

// Tests WebState and NavigationManager correctly return native content URL and
// VirtualURL
TEST_F(CRWWebControllerNativeContentTest, NativeContentVirtualURL) {
  GURL url_to_load(kTestAppSpecificURL);
  GURL virtual_url(kTestURLString);
  base::scoped_nsobject<TestNativeContent> content([[TestNativeContent alloc]
      initWithURL:virtual_url
       virtualURL:virtual_url]);
  [mock_native_provider_ setController:content forURL:url_to_load];
  Load(url_to_load);
  web::URLVerificationTrustLevel trust_level = web::kNone;
  GURL gurl = [web_controller() currentURLWithTrustLevel:&trust_level];
  EXPECT_EQ(gurl, virtual_url);
  EXPECT_EQ(web::kAbsolute, trust_level);
  EXPECT_EQ([web_controller() webState]->GetVisibleURL(), virtual_url);
  NavigationManagerImpl& navigationManager =
      [web_controller() webStateImpl]->GetNavigationManagerImpl();
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetVirtualURL(), virtual_url);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetVirtualURL(),
            virtual_url);
}

// A separate test class, as none of the |CRWUIWebViewWebControllerTest| setup
// is needed;
typedef web::WebTestWithWebController CRWWebControllerObserversTest;

// Tests that CRWWebControllerObservers are called.
TEST_F(CRWWebControllerObserversTest, Observers) {
  base::scoped_nsobject<CountingObserver> observer(
      [[CountingObserver alloc] init]);
  EXPECT_EQ(0u, [web_controller() observerCount]);
  [web_controller() addObserver:observer];
  EXPECT_EQ(1u, [web_controller() observerCount]);

  EXPECT_EQ(0, [observer pageLoadedCount]);
  [web_controller() webStateImpl]->OnPageLoaded(GURL("http://test"), false);
  EXPECT_EQ(0, [observer pageLoadedCount]);
  [web_controller() webStateImpl]->OnPageLoaded(GURL("http://test"), true);
  EXPECT_EQ(1, [observer pageLoadedCount]);

  EXPECT_EQ(0, [observer messageCount]);
  // Non-matching prefix.
  EXPECT_FALSE([web_controller() webStateImpl]->OnScriptCommandReceived(
      "a", base::DictionaryValue(), GURL("http://test"), true));
  EXPECT_EQ(0, [observer messageCount]);
  // Matching prefix.
  EXPECT_TRUE([web_controller() webStateImpl]->OnScriptCommandReceived(
      base::SysNSStringToUTF8([observer commandPrefix]) + ".foo",
      base::DictionaryValue(), GURL("http://test"), true));
  EXPECT_EQ(1, [observer messageCount]);

  [web_controller() removeObserver:observer];
  EXPECT_EQ(0u, [web_controller() observerCount]);
};

// Test fixture for window.open tests.
class CRWWebControllerWindowOpenTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();

    // Configure web delegate.
    delegate_.reset([[MockInteractionLoader alloc]
        initWithRepresentedObject:
            [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)]]);
    ASSERT_TRUE([delegate_ blockPopups]);
    [web_controller() setDelegate:delegate_];

    // Configure child web state.
    child_web_state_.reset(new web::WebStateImpl(GetBrowserState()));
    child_web_state_->SetWebUsageEnabled(true);
    [delegate_ setChildWebController:child_web_state_->GetWebController()];

    // Configure child web controller's session controller mock.
    id sessionController =
        [OCMockObject niceMockForClass:[CRWSessionController class]];
    BOOL yes = YES;
    [[[sessionController stub] andReturnValue:OCMOCK_VALUE(yes)] isOpenedByDOM];
    child_web_state_->GetNavigationManagerImpl().SetSessionController(
        sessionController);

    LoadHtml(@"<html><body></body></html>");
  }
  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
    [web_controller() setDelegate:nil];

    web::WebTestWithWebController::TearDown();
  }
  // Executes JavaScript that opens a new window and returns evaluation result
  // as a string.
  id OpenWindowByDOM() {
    NSString* const kOpenWindowScript =
        @"var w = window.open('javascript:void(0);', target='_blank');"
         "w ? w.toString() : null;";
    id windowJSObject = ExecuteJavaScript(kOpenWindowScript);
    WaitForBackgroundTasks();
    return windowJSObject;
  }
  // A CRWWebDelegate mock used for testing.
  base::scoped_nsobject<id> delegate_;
  // A child WebState used for testing.
  std::unique_ptr<web::WebStateImpl> child_web_state_;
};

// Tests that absence of web delegate is handled gracefully.
TEST_F(CRWWebControllerWindowOpenTest, NoDelegate) {
  [web_controller() setDelegate:nil];

  EXPECT_NSEQ([NSNull null], OpenWindowByDOM());

  EXPECT_FALSE([delegate_ blockedPopupInfo]);
}

// Tests that window.open triggered by user gesture opens a new non-popup
// window.
TEST_F(CRWWebControllerWindowOpenTest, OpenWithUserGesture) {
  SEL selector = @selector(webPageOrderedOpen);
  [delegate_ onSelector:selector
      callBlockExpectation:^(){
      }];

  [web_controller() touched:YES];
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

  ASSERT_FALSE([web_controller() userIsInteracting]);
  EXPECT_NSEQ([NSNull null], OpenWindowByDOM());
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
  ASSERT_FALSE([web_controller() userIsInteracting]);
  EXPECT_NSEQ([NSNull null], OpenWindowByDOM());
  base::test::ios::WaitUntilCondition(^bool() {
    return [delegate_ blockedPopupInfo];
  });

  EXPECT_EQ("", [delegate_ sourceURL].spec());
  EXPECT_EQ("javascript:void(0);", [delegate_ popupURL].spec());
};

// Fixture class to test WKWebView crashes.
class CRWWebControllerWebProcessTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    webView_.reset(web::CreateTerminatedWKWebView());
    base::scoped_nsobject<TestWebViewContentView> webViewContentView(
        [[TestWebViewContentView alloc]
            initWithMockWebView:webView_
                     scrollView:[webView_ scrollView]]);
    [web_controller() injectWebViewContentView:webViewContentView];
  }
  base::scoped_nsobject<WKWebView> webView_;
};

// Tests that -[CRWWebDelegate webControllerWebProcessDidCrash:] is called
// when WKWebView web process has crashed.
TEST_F(CRWWebControllerWebProcessTest, Crash) {
  id delegate = [OCMockObject niceMockForProtocol:@protocol(CRWWebDelegate)];
  [[delegate expect] webControllerWebProcessDidCrash:web_controller()];

  [web_controller() setDelegate:delegate];
  web::SimulateWKWebViewCrash(webView_);

  EXPECT_OCMOCK_VERIFY(delegate);
  EXPECT_FALSE([web_controller() isViewAlive]);
  [web_controller() setDelegate:nil];
};

}  // namespace
