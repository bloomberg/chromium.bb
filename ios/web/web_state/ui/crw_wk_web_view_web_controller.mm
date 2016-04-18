// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_web_view_web_controller.h"

#include <stddef.h>

#include <utility>

#include "base/ios/weak_nsobject.h"
#include "base/json/json_reader.h"
#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/url_util.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_kit_constants.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/js/crw_js_injection_manager.h"
#import "ios/web/public/web_state/ui/crw_native_content_provider.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#import "ios/web/web_state/error_translation_util.h"
#include "ios/web/web_state/frame_info.h"
#import "ios/web/web_state/js/crw_js_window_id_manager.h"
#import "ios/web/web_state/ui/crw_web_controller+protected.h"
#import "ios/web/web_state/ui/crw_wk_script_message_router.h"
#import "ios/web/web_state/ui/web_view_js_utils.h"
#import "ios/web/web_state/web_state_impl.h"
#import "ios/web/web_state/wk_web_view_security_util.h"
#import "net/base/mac/url_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"
#include "url/url_constants.h"

@implementation CRWWKWebViewWebController

#pragma mark Protected method implementations

// The core.js cannot pass messages back to obj-c  if it is injected
// to |WEB_VIEW_DOCUMENT| because it does not support iframe creation used
// by core.js to communicate back. That functionality is only supported
// by |WEB_VIEW_HTML_DOCUMENT|. |WEB_VIEW_DOCUMENT| is used when displaying
// non-HTML contents (e.g. PDF documents).
- (web::WebViewDocumentType)webViewDocumentType {
  // This happens during tests.
  if (!self.webView) {
    return web::WEB_VIEW_DOCUMENT_TYPE_GENERIC;
  }

  std::string MIMEType = self.webState->GetContentsMimeType();
  return [self documentTypeFromMIMEType:base::SysUTF8ToNSString(MIMEType)];
}

- (void)willLoadCurrentURLInWebView {
  // TODO(stuartmorgan): Get a WKWebView version of the request ID verification
  // code working for debug builds.
}

// Overrides the hashchange workaround in the super class that manually
// triggers Javascript hashchange events. If navigating with native API,
// i.e. using a back forward list item, hashchange events will be triggered
// automatically, so no URL tampering is required.
- (GURL)URLForHistoryNavigationFromItem:(web::NavigationItem*)fromItem
                                 toItem:(web::NavigationItem*)toItem {
  web::WKBackForwardListItemHolder* holder =
      web::WKBackForwardListItemHolder::FromNavigationItem(toItem);

  if (holder->back_forward_list_item()) {
    return toItem->GetURL();
  }
  return [super URLForHistoryNavigationFromItem:fromItem toItem:toItem];
}

- (void)applyWebViewScrollZoomScaleFromZoomState:
    (const web::PageZoomState&)zoomState {
  // After rendering a web page, WKWebView keeps the |minimumZoomScale| and
  // |maximumZoomScale| properties of its scroll view constant while adjusting
  // the |zoomScale| property accordingly.  The maximum-scale or minimum-scale
  // meta tags of a page may have changed since the state was recorded, so clamp
  // the zoom scale to the current range if necessary.
  DCHECK(zoomState.IsValid());
  CGFloat zoomScale = zoomState.zoom_scale();
  if (zoomScale < self.webScrollView.minimumZoomScale)
    zoomScale = self.webScrollView.minimumZoomScale;
  if (zoomScale > self.webScrollView.maximumZoomScale)
    zoomScale = self.webScrollView.maximumZoomScale;
  self.webScrollView.zoomScale = zoomScale;
}

#pragma mark Private methods

- (void)webViewWebProcessDidCrash {
  [self setWebProcessIsDead:YES];

  SEL cancelDialogsSelector = @selector(cancelDialogsForWebController:);
  if ([self.UIDelegate respondsToSelector:cancelDialogsSelector])
    [self.UIDelegate cancelDialogsForWebController:self];

  SEL rendererCrashSelector = @selector(webControllerWebProcessDidCrash:);
  if ([self.delegate respondsToSelector:rendererCrashSelector])
    [self.delegate webControllerWebProcessDidCrash:self];
}

#pragma mark -
#pragma mark CRWWebViewScrollViewProxyObserver

// Under WKWebView, JavaScript can execute asynchronously. User can start
// scrolling and calls to window.scrollTo executed during scrolling will be
// treated as "during user interaction" and can cause app to go fullscreen.
// This is a workaround to use this webViewScrollViewIsDragging flag to ignore
// window.scrollTo while user is scrolling. See crbug.com/554257
- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self evaluateJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(true)"
       stringResultHandler:nil];
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  [self evaluateJavaScript:@"__gCrWeb.setWebViewScrollViewIsDragging(false)"
       stringResultHandler:nil];
}

@end
