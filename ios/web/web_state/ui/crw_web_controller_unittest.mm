// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <WebKit/WebKit.h>

#include <utility>

#include "base/ios/ios_util.h"
#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/test/fakes/test_native_content.h"
#import "ios/web/public/test/fakes/test_native_content_provider.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/fakes/test_web_state_delegate.h"
#include "ios/web/public/test/fakes/test_web_state_observer.h"
#import "ios/web/public/test/fakes/test_web_view_content_view.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "ios/web/test/wk_web_view_crash_utils.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/test/ios/ui_view_test_utils.h"

using web::NavigationManagerImpl;

@interface CRWWebController (PrivateAPI)
@property(nonatomic, readwrite) web::PageDisplayState pageDisplayState;
@end

@interface CountingObserver : NSObject<CRWWebControllerObserver>

@property(nonatomic, readonly) int pageLoadedCount;
@end

@implementation CountingObserver
@synthesize pageLoadedCount = _pageLoadedCount;

- (void)pageLoaded:(CRWWebController*)webController {
  ++_pageLoadedCount;
}

@end

namespace {

// Syntactically invalid URL per rfc3986.
const char kInvalidURL[] = "http://%3";

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

// Test fixture for testing CRWWebController. Stubs out web view.
class CRWWebControllerTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    mock_web_view_.reset([CreateMockWebView() retain]);
    scroll_view_.reset([[UIScrollView alloc] init]);
    [[[mock_web_view_ stub] andReturn:scroll_view_.get()] scrollView];

    base::scoped_nsobject<TestWebViewContentView> webViewContentView(
        [[TestWebViewContentView alloc] initWithMockWebView:mock_web_view_
                                                 scrollView:scroll_view_]);
    [web_controller() injectWebViewContentView:webViewContentView];
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mock_web_view_);
    [web_controller() resetInjectedWebViewContentView];
    web::WebTestWithWebController::TearDown();
  }

  // The value for web view OCMock objects to expect for |-setFrame:|.
  CGRect GetExpectedWebViewFrame() const {
    CGSize container_view_size = [UIScreen mainScreen].bounds.size;
    container_view_size.height -=
        CGRectGetHeight([UIApplication sharedApplication].statusBarFrame);
    return {CGPointZero, container_view_size};
  }

  // Creates WebView mock.
  UIView* CreateMockWebView() {
    id result = [OCMockObject mockForClass:[WKWebView class]];

    if (base::ios::IsRunningOnIOS10OrLater()) {
      [[result stub] serverTrust];
    } else {
      [[result stub] certificateChain];
    }

    [[result stub] backForwardList];
    [[[result stub] andReturn:[NSURL URLWithString:@(kTestURLString)]] URL];
    [[result stub] setNavigationDelegate:[OCMArg checkWithBlock:^(id delegate) {
                     navigation_delegate_.reset(delegate);
                     return YES;
                   }]];
    [[result stub] setUIDelegate:OCMOCK_ANY];
    [[result stub] setFrame:GetExpectedWebViewFrame()];
    [[result stub] addObserver:web_controller()
                    forKeyPath:OCMOCK_ANY
                       options:0
                       context:nullptr];
    [[result stub] removeObserver:web_controller() forKeyPath:OCMOCK_ANY];

    return result;
  }

  base::WeakNSProtocol<id<WKNavigationDelegate>> navigation_delegate_;
  base::scoped_nsobject<UIScrollView> scroll_view_;
  base::scoped_nsobject<id> mock_web_view_;
};

// Tests that AllowCertificateError is called with correct arguments if
// WKWebView fails to load a page with bad SSL cert.
TEST_F(CRWWebControllerTest, SslCertError) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_change_visible_security_state_info());

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
  base::scoped_nsobject<NSObject> navigation([[NSObject alloc] init]);
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:static_cast<WKNavigation*>(navigation)];
  [navigation_delegate_ webView:mock_web_view_
      didFailProvisionalNavigation:static_cast<WKNavigation*>(navigation)
                         withError:error];

  // Verify correctness of AllowCertificateError method call.
  EXPECT_EQ(net::ERR_CERT_INVALID, GetWebClient()->last_cert_error_code());
  EXPECT_TRUE(GetWebClient()->last_cert_error_ssl_info().is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID,
            GetWebClient()->last_cert_error_ssl_info().cert_status);
  EXPECT_EQ(url, GetWebClient()->last_cert_error_request_url());
  EXPECT_FALSE(GetWebClient()->last_cert_error_overridable());

  // Verify that |DidChangeVisibleSecurityState| was called.
  ASSERT_TRUE(observer.did_change_visible_security_state_info());
  EXPECT_EQ(web_state(),
            observer.did_change_visible_security_state_info()->web_state);
}

// Test fixture to test |WebState::SetShouldSuppressDialogs|.
class DialogsSuppressionTest : public web::WebTestWithWebState {
 protected:
  DialogsSuppressionTest() : page_url_("https://chromium.test/") {}
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    LoadHtml(@"<html><body></body></html>", page_url_);
    web_state()->SetDelegate(&test_web_delegate_);
  }
  void TearDown() override {
    web_state()->SetDelegate(nullptr);
    web::WebTestWithWebState::TearDown();
  }
  web::TestJavaScriptDialogPresenter* js_dialog_presenter() {
    return test_web_delegate_.GetTestJavaScriptDialogPresenter();
  }
  const std::vector<web::TestJavaScriptDialog>& requested_dialogs() {
    return js_dialog_presenter()->requested_dialogs();
  }
  const GURL& page_url() { return page_url_; }

 private:
  web::TestWebStateDelegate test_web_delegate_;
  GURL page_url_;
};

// Tests that window.alert dialog is suppressed.
TEST_F(DialogsSuppressionTest, SuppressAlert) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_suppress_dialog_info());
  web_state()->SetShouldSuppressDialogs(true);
  ExecuteJavaScript(@"alert('test')");
  ASSERT_TRUE(observer.did_suppress_dialog_info());
  EXPECT_EQ(web_state(), observer.did_suppress_dialog_info()->web_state);
};

// Tests that window.alert dialog is shown.
TEST_F(DialogsSuppressionTest, AllowAlert) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_suppress_dialog_info());
  ASSERT_TRUE(requested_dialogs().empty());

  web_state()->SetShouldSuppressDialogs(false);
  ExecuteJavaScript(@"alert('test')");

  ASSERT_EQ(1U, requested_dialogs().size());
  web::TestJavaScriptDialog dialog = requested_dialogs()[0];
  EXPECT_EQ(web_state(), dialog.web_state);
  EXPECT_EQ(page_url(), dialog.origin_url);
  EXPECT_EQ(web::JAVASCRIPT_DIALOG_TYPE_ALERT, dialog.java_script_dialog_type);
  EXPECT_NSEQ(@"test", dialog.message_text);
  EXPECT_FALSE(dialog.default_prompt_text);
  ASSERT_FALSE(observer.did_suppress_dialog_info());
};

// Tests that window.confirm dialog is suppressed.
TEST_F(DialogsSuppressionTest, SuppressConfirm) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_suppress_dialog_info());
  ASSERT_TRUE(requested_dialogs().empty());

  web_state()->SetShouldSuppressDialogs(true);
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_TRUE(requested_dialogs().empty());
  ASSERT_TRUE(observer.did_suppress_dialog_info());
  EXPECT_EQ(web_state(), observer.did_suppress_dialog_info()->web_state);
};

// Tests that window.confirm dialog is shown and its result is true.
TEST_F(DialogsSuppressionTest, AllowConfirmWithTrue) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_suppress_dialog_info());
  ASSERT_TRUE(requested_dialogs().empty());

  js_dialog_presenter()->set_callback_success_argument(true);

  web_state()->SetShouldSuppressDialogs(false);
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_EQ(1U, requested_dialogs().size());
  web::TestJavaScriptDialog dialog = requested_dialogs()[0];
  EXPECT_EQ(web_state(), dialog.web_state);
  EXPECT_EQ(page_url(), dialog.origin_url);
  EXPECT_EQ(web::JAVASCRIPT_DIALOG_TYPE_CONFIRM,
            dialog.java_script_dialog_type);
  EXPECT_NSEQ(@"test", dialog.message_text);
  EXPECT_FALSE(dialog.default_prompt_text);
  ASSERT_FALSE(observer.did_suppress_dialog_info());
}

// Tests that window.confirm dialog is shown and its result is false.
TEST_F(DialogsSuppressionTest, AllowConfirmWithFalse) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_suppress_dialog_info());
  ASSERT_TRUE(requested_dialogs().empty());

  web_state()->SetShouldSuppressDialogs(false);
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_EQ(1U, requested_dialogs().size());
  web::TestJavaScriptDialog dialog = requested_dialogs()[0];
  EXPECT_EQ(web_state(), dialog.web_state);
  EXPECT_EQ(page_url(), dialog.origin_url);
  EXPECT_EQ(web::JAVASCRIPT_DIALOG_TYPE_CONFIRM,
            dialog.java_script_dialog_type);
  EXPECT_NSEQ(@"test", dialog.message_text);
  EXPECT_FALSE(dialog.default_prompt_text);
  ASSERT_FALSE(observer.did_suppress_dialog_info());
}

// Tests that window.prompt dialog is suppressed.
TEST_F(DialogsSuppressionTest, SuppressPrompt) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_suppress_dialog_info());
  ASSERT_TRUE(requested_dialogs().empty());

  web_state()->SetShouldSuppressDialogs(true);
  EXPECT_EQ([NSNull null], ExecuteJavaScript(@"prompt('Yes?', 'No')"));

  ASSERT_TRUE(requested_dialogs().empty());
  ASSERT_TRUE(observer.did_suppress_dialog_info());
  EXPECT_EQ(web_state(), observer.did_suppress_dialog_info()->web_state);
}

// Tests that window.prompt dialog is shown.
TEST_F(DialogsSuppressionTest, AllowPrompt) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_suppress_dialog_info());
  ASSERT_TRUE(requested_dialogs().empty());

  js_dialog_presenter()->set_callback_user_input_argument(@"Maybe");

  web_state()->SetShouldSuppressDialogs(false);
  EXPECT_NSEQ(@"Maybe", ExecuteJavaScript(@"prompt('Yes?', 'No')"));

  ASSERT_EQ(1U, requested_dialogs().size());
  web::TestJavaScriptDialog dialog = requested_dialogs()[0];
  EXPECT_EQ(web_state(), dialog.web_state);
  EXPECT_EQ(page_url(), dialog.origin_url);
  EXPECT_EQ(web::JAVASCRIPT_DIALOG_TYPE_PROMPT, dialog.java_script_dialog_type);
  EXPECT_NSEQ(@"Yes?", dialog.message_text);
  EXPECT_NSEQ(@"No", dialog.default_prompt_text);
  ASSERT_FALSE(observer.did_suppress_dialog_info());
}

// Tests that window.open is suppressed.
TEST_F(DialogsSuppressionTest, SuppressWindowOpen) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_suppress_dialog_info());
  ASSERT_TRUE(requested_dialogs().empty());

  web_state()->SetShouldSuppressDialogs(true);
  ExecuteJavaScript(@"window.open('')");

  ASSERT_TRUE(observer.did_suppress_dialog_info());
  EXPECT_EQ(web_state(), observer.did_suppress_dialog_info()->web_state);
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

// TODO(crbug/493427): Flaky on the bots.
TEST_F(CRWWebControllerPageScrollStateTest,
       FLAKY_SetPageDisplayStateWithUserScalableDisabled) {
#if !TARGET_IPHONE_SIMULATOR
  // TODO(crbug.com/493427): fails flakily on device, so skip it there.
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

// TODO(crbug/493427): Flaky on the bots.
TEST_F(CRWWebControllerPageScrollStateTest,
       FLAKY_SetPageDisplayStateWithUserScalableEnabled) {
#if !TARGET_IPHONE_SIMULATOR
  // TODO(crbug.com/493427): fails flakily on device, so skip it there.
  return;
#endif
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

// TODO(crbug/493427): Flaky on the bots.
TEST_F(CRWWebControllerPageScrollStateTest, DISABLED_AtTop) {
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
TEST_F(CRWWebControllerNavigationTest, GoToItemWithoutDocumentChange) {
  LoadHtml(@"<html><body></body></html>", GURL("https://chromium.test"));
  LoadHtml(@"<html><body></body></html>", GURL("https://chromium.test#hash"));
  NavigationManagerImpl& nav_manager =
      web_controller().webStateImpl->GetNavigationManagerImpl();
  CRWSessionController* session_controller = nav_manager.GetSessionController();
  EXPECT_EQ(2U, session_controller.items.size());
  EXPECT_EQ(session_controller.items.back().get(),
            session_controller.currentItem);

  [web_controller() goToItemAtIndex:0];
  EXPECT_EQ(session_controller.items.front().get(),
            session_controller.currentItem);
}

// Test fixture for testing visible security state.
typedef web::WebTestWithWebState CRWWebStateSecurityStateTest;

// Tests that OnPasswordInputShownOnHttp updates the SSLStatus to indicate that
// a password field has been displayed on an HTTP page.
TEST_F(CRWWebStateSecurityStateTest, HttpPassword) {
  LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  web::NavigationManager* nav_manager = web_state()->GetNavigationManager();
  EXPECT_FALSE(nav_manager->GetLastCommittedItem()->GetSSL().content_status &
               web::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_change_visible_security_state_info());
  web_state()->OnPasswordInputShownOnHttp();
  EXPECT_TRUE(nav_manager->GetLastCommittedItem()->GetSSL().content_status &
              web::SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  ASSERT_TRUE(observer.did_change_visible_security_state_info());
  EXPECT_EQ(web_state(),
            observer.did_change_visible_security_state_info()->web_state);
}

// Tests that OnCreditCardInputShownOnHttp updates the SSLStatus to indicate
// that a credit card field has been displayed on an HTTP page.
TEST_F(CRWWebStateSecurityStateTest, HttpCreditCard) {
  LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  web::NavigationManager* nav_manager = web_state()->GetNavigationManager();
  EXPECT_FALSE(nav_manager->GetLastCommittedItem()->GetSSL().content_status &
               web::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_change_visible_security_state_info());
  web_state()->OnCreditCardInputShownOnHttp();
  EXPECT_TRUE(nav_manager->GetLastCommittedItem()->GetSSL().content_status &
              web::SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
  ASSERT_TRUE(observer.did_change_visible_security_state_info());
  EXPECT_EQ(web_state(),
            observer.did_change_visible_security_state_info()->web_state);
}

// Tests that loading HTTP page updates the SSLStatus.
TEST_F(CRWWebStateSecurityStateTest, LoadHttpPage) {
  web::TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_change_visible_security_state_info());
  LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  web::NavigationManager* nav_manager = web_state()->GetNavigationManager();
  web::NavigationItem* item = nav_manager->GetLastCommittedItem();
  EXPECT_EQ(web::SECURITY_STYLE_UNAUTHENTICATED, item->GetSSL().security_style);
  ASSERT_TRUE(observer.did_change_visible_security_state_info());
  EXPECT_EQ(web_state(),
            observer.did_change_visible_security_state_info()->web_state);
}

// Real WKWebView is required for CRWWebControllerInvalidUrlTest.
typedef web::WebTestWithWebState CRWWebControllerInvalidUrlTest;

// Tests that web controller does not navigate to about:blank if iframe src
// has invalid url. Web controller loads about:blank if page navigates to
// invalid url, but should do nothing if navigation is performed in iframe. This
// test prevents crbug.com/694865 regression.
TEST_F(CRWWebControllerInvalidUrlTest, IFrameWithInvalidURL) {
  GURL url("http://chromium.test");
  ASSERT_FALSE(GURL(kInvalidURL).is_valid());
  LoadHtml([NSString stringWithFormat:@"<iframe src='%s'/>", kInvalidURL], url);
  EXPECT_EQ(url, web_state()->GetLastCommittedURL());
}

// Real WKWebView is required for CRWWebControllerFormActivityTest.
typedef web::WebTestWithWebState CRWWebControllerFormActivityTest;

// Tests that keyup event correctly delivered to WebStateObserver.
TEST_F(CRWWebControllerFormActivityTest, KeyUpEvent) {
  web::TestWebStateObserver observer(web_state());
  LoadHtml(@"<p></p>");
  ASSERT_FALSE(observer.form_activity_info());
  ExecuteJavaScript(@"document.dispatchEvent(new KeyboardEvent('keyup'));");
  web::TestFormActivityInfo* info = observer.form_activity_info();
  ASSERT_TRUE(info);
  EXPECT_EQ("keyup", info->type);
  EXPECT_FALSE(info->input_missing);
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

// Tests |currentURLWithTrustLevel:| method.
TEST_F(CRWWebControllerTest, CurrentUrlWithTrustLevel) {
  AddPendingItem(GURL("http://chromium.test"), ui::PAGE_TRANSITION_TYPED);

  [[[mock_web_view_ stub] andReturnBool:NO] hasOnlySecureContent];
  [[[mock_web_view_ stub] andReturn:@""] title];

  // Stub out the injection process.
  [[mock_web_view_ stub] evaluateJavaScript:OCMOCK_ANY
                          completionHandler:OCMOCK_ANY];

  // Simulate a page load to trigger a URL update.
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:nil];
  [navigation_delegate_ webView:mock_web_view_ didCommitNavigation:nil];

  web::URLVerificationTrustLevel trust_level = web::kNone;
  GURL url = [web_controller() currentURLWithTrustLevel:&trust_level];

  EXPECT_EQ(GURL(kTestURLString), url);
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
    navigation_manager.AddPendingItem(
        URL, web::Referrer(), ui::PAGE_TRANSITION_TYPED,
        web::NavigationInitiationType::USER_INITIATED,
        web::NavigationManager::UserAgentOverrideOption::INHERIT);
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

  [web_controller() removeObserver:observer];
  EXPECT_EQ(0u, [web_controller() observerCount]);
};

// Test fixture for window.open tests.
class WindowOpenByDomTest : public web::WebTestWithWebController {
 protected:
  WindowOpenByDomTest() : opener_url_("http://test") {}

  void SetUp() override {
    WebTestWithWebController::SetUp();
    web_state()->SetDelegate(&delegate_);
    LoadHtml(@"<html><body></body></html>", opener_url_);
  }
  // Executes JavaScript that opens a new window and returns evaluation result
  // as a string.
  id OpenWindowByDom() {
    NSString* const kOpenWindowScript =
        @"w = window.open('javascript:void(0);', target='_blank');"
         "w ? w.toString() : null;";
    id windowJSObject = ExecuteJavaScript(kOpenWindowScript);
    return windowJSObject;
  }

  // Executes JavaScript that closes previously opened window.
  void CloseWindow() { ExecuteJavaScript(@"w.close()"); }

  // URL of a page which opens child windows.
  const GURL opener_url_;
  web::TestWebStateDelegate delegate_;
};

// Tests that absence of web state delegate is handled gracefully.
TEST_F(WindowOpenByDomTest, NoDelegate) {
  web_state()->SetDelegate(nullptr);

  EXPECT_NSEQ([NSNull null], OpenWindowByDom());

  EXPECT_TRUE(delegate_.child_windows().empty());
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that window.open triggered by user gesture opens a new non-popup
// window.
TEST_F(WindowOpenByDomTest, OpenWithUserGesture) {
  [web_controller() touched:YES];
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDom());

  ASSERT_EQ(1U, delegate_.child_windows().size());
  ASSERT_TRUE(delegate_.child_windows()[0]);
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that window.open executed w/o user gesture does not open a new window,
// but blocks popup instead.
TEST_F(WindowOpenByDomTest, BlockPopup) {
  ASSERT_FALSE([web_controller() userIsInteracting]);
  EXPECT_NSEQ([NSNull null], OpenWindowByDom());

  EXPECT_TRUE(delegate_.child_windows().empty());
  ASSERT_EQ(1U, delegate_.popups().size());
  EXPECT_EQ(GURL("javascript:void(0);"), delegate_.popups()[0].url);
  EXPECT_EQ(opener_url_, delegate_.popups()[0].opener_url);
}

// Tests that window.open executed w/o user gesture opens a new window, assuming
// that delegate allows popups.
TEST_F(WindowOpenByDomTest, DontBlockPopup) {
  delegate_.allow_popups(opener_url_);
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDom());

  ASSERT_EQ(1U, delegate_.child_windows().size());
  ASSERT_TRUE(delegate_.child_windows()[0]);
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that window.close closes the web state.
TEST_F(WindowOpenByDomTest, CloseWindow) {
  delegate_.allow_popups(opener_url_);
  ASSERT_NSEQ(@"[object Window]", OpenWindowByDom());

  ASSERT_EQ(1U, delegate_.child_windows().size());
  ASSERT_TRUE(delegate_.child_windows()[0]);
  EXPECT_TRUE(delegate_.popups().empty());

  delegate_.child_windows()[0]->SetDelegate(&delegate_);
  CloseWindow();

  EXPECT_TRUE(delegate_.child_windows().empty());
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests page title changes.
typedef web::WebTestWithWebState CRWWebControllerTitleTest;
TEST_F(CRWWebControllerTitleTest, TitleChange) {
  // Observes and waits for TitleWasSet call.
  class TitleObserver : public web::WebStateObserver {
   public:
    explicit TitleObserver(web::WebState* web_state)
        : web::WebStateObserver(web_state) {}
    // Returns number of times |TitleWasSet| was called.
    int title_change_count() { return title_change_count_; }
    // WebStateObserver overrides:
    void TitleWasSet() override { title_change_count_++; }

   private:
    int title_change_count_ = 0;
  };

  TitleObserver observer(web_state());
  ASSERT_EQ(0, observer.title_change_count());

  // Expect TitleWasSet callback after the page is loaded.
  LoadHtml(@"<title>Title1</title>");
  EXPECT_EQ("Title1", base::UTF16ToUTF8(web_state()->GetTitle()));
  EXPECT_EQ(1, observer.title_change_count());

  // Expect at least one more TitleWasSet callback after changing title via
  // JavaScript. On iOS 10 WKWebView fires 3 callbacks after JS excucution
  // with the following title changes: "Title2", "" and "Title2".
  // TODO(crbug.com/696104): There should be only 2 calls of TitleWasSet.
  // Fix expecteation when WKWebView stops sending extra KVO calls.
  ExecuteJavaScript(@"window.document.title = 'Title2';");
  EXPECT_EQ("Title2", base::UTF16ToUTF8(web_state()->GetTitle()));
  EXPECT_GE(observer.title_change_count(), 2);
};

// Tests that fragment change navigations use title from the previous page.
TEST_F(CRWWebControllerTitleTest, FragmentChangeNavigationsUsePreviousTitle) {
  LoadHtml(@"<title>Title1</title>");
  ASSERT_EQ("Title1", base::UTF16ToUTF8(web_state()->GetTitle()));
  ExecuteJavaScript(@"window.location.hash = '#1'");
  EXPECT_EQ("Title1", base::UTF16ToUTF8(web_state()->GetTitle()));
}

// Test fixture for JavaScript execution.
class ScriptExecutionTest : public web::WebTestWithWebController {
 protected:
  // Calls |executeUserJavaScript:completionHandler:|, waits for script
  // execution completion, and synchronously returns the result.
  id ExecuteUserJavaScript(NSString* java_script, NSError** error) {
    __block id script_result = nil;
    __block NSError* script_error = nil;
    __block bool script_executed = false;
    [web_controller()
        executeUserJavaScript:java_script
            completionHandler:^(id local_result, NSError* local_error) {
              script_result = [local_result retain];
              script_error = [local_error retain];
              script_executed = true;
            }];

    WaitForCondition(^{
      return script_executed;
    });

    if (error) {
      *error = script_error;
    }
    [script_error autorelease];
    return [script_result autorelease];
  }
};

// Tests evaluating user script on an http page.
TEST_F(ScriptExecutionTest, UserScriptOnHttpPage) {
  LoadHtml(@"<html></html>", GURL(kTestURLString));
  NSError* error = nil;
  EXPECT_NSEQ(@0, ExecuteUserJavaScript(@"window.w = 0;", &error));
  EXPECT_FALSE(error);

  EXPECT_NSEQ(@0, ExecuteJavaScript(@"window.w"));
};

// Tests evaluating user script on app-specific page. Pages with app-specific
// URLs have elevated privileges and JavaScript execution should not be allowed
// for them.
TEST_F(ScriptExecutionTest, UserScriptOnAppSpecificPage) {
  LoadHtml(@"<html></html>", GURL(kTestURLString));

  // Change last committed URL to app-specific URL.
  web::NavigationManagerImpl& nav_manager =
      [web_controller() webStateImpl]->GetNavigationManagerImpl();
  nav_manager.AddPendingItem(
      GURL(kTestAppSpecificURL), web::Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::USER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [nav_manager.GetSessionController() commitPendingItem];

  NSError* error = nil;
  EXPECT_FALSE(ExecuteUserJavaScript(@"window.w = 0;", &error));
  ASSERT_TRUE(error);
  EXPECT_NSEQ(web::kJSEvaluationErrorDomain, error.domain);
  EXPECT_EQ(web::JS_EVALUATION_ERROR_CODE_NO_WEB_VIEW, error.code);

  EXPECT_FALSE(ExecuteJavaScript(@"window.w"));
};

// Fixture class to test WKWebView crashes.
class CRWWebControllerWebProcessTest : public web::WebTestWithWebController {
 protected:
  void SetUp() override {
    web::WebTestWithWebController::SetUp();
    webView_.reset([web::BuildTerminatedWKWebView() retain]);
    base::scoped_nsobject<TestWebViewContentView> webViewContentView(
        [[TestWebViewContentView alloc]
            initWithMockWebView:webView_
                     scrollView:[webView_ scrollView]]);
    [web_controller() injectWebViewContentView:webViewContentView];

    // This test intentionally crashes the render process.
    SetIgnoreRenderProcessCrashesDuringTesting(true);
  }
  base::scoped_nsobject<WKWebView> webView_;
};

// Tests that WebStateDelegate::RenderProcessGone is called when WKWebView web
// process has crashed.
TEST_F(CRWWebControllerWebProcessTest, Crash) {
  web::TestWebStateObserver observer(web_state());
  web::TestWebStateObserver* observer_ptr = &observer;
  web::SimulateWKWebViewCrash(webView_);
  base::test::ios::WaitUntilCondition(^bool() {
    return observer_ptr->render_process_gone_info();
  });
  EXPECT_EQ(web_state(), observer.render_process_gone_info()->web_state);
  EXPECT_FALSE([web_controller() isViewAlive]);
};

}  // namespace
