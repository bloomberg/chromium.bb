// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/static_content/static_html_native_content.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#include "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/web/public/referrer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface StaticHtmlNativeContent ()
// Designated initializer.
- (instancetype)initWithLoader:(id<UrlLoader>)loader
      staticHTMLViewController:(StaticHtmlViewController*)HTMLViewController
                           URL:(const GURL&)URL;
@end

@implementation StaticHtmlNativeContent {
  // The url of the controller.
  GURL _URL;
  // YES is web views are allowed to be created.
  BOOL _webUsageEnabled;
  // The static HTML view controller that is used to display the content in
  // a web view.
  StaticHtmlViewController* _staticHTMLViewController;
  // Responsible for loading a particular URL.
  __weak id<UrlLoader> _loader;  // weak
  // The controller handling the overscroll actions.
  OverscrollActionsController* _overscrollActionsController;
}

#pragma mark -
#pragma mark Public

- (instancetype)initWithLoader:(id<UrlLoader>)loader
      staticHTMLViewController:(StaticHtmlViewController*)HTMLViewController
                           URL:(const GURL&)URL {
  DCHECK(loader);
  DCHECK(HTMLViewController);
  // No DCHECK for URL (invalid URL is a valid input).
  if (self = [super init]) {
    web::Referrer referrer(URL, web::ReferrerPolicyDefault);
    [HTMLViewController setLoader:loader referrer:referrer];
    _URL = URL;
    _loader = loader;
    _staticHTMLViewController = HTMLViewController;
  }
  return self;
}

- (instancetype)initWithResourcePathResource:(NSString*)resourcePath
                                      loader:(id<UrlLoader>)loader
                                browserState:(web::BrowserState*)browserState
                                         url:(const GURL&)URL {
  DCHECK(loader);
  DCHECK(browserState);
  DCHECK(URL.is_valid());
  DCHECK(resourcePath);
  StaticHtmlViewController* HTMLViewController =
      [[StaticHtmlViewController alloc] initWithResource:resourcePath
                                            browserState:browserState];
  return [self initWithLoader:loader
      staticHTMLViewController:HTMLViewController
                           URL:URL];
}

- (void)dealloc {
  [[self scrollView] setDelegate:nil];
}

- (void)loadURLWithParams:(const web::NavigationManager::WebLoadParams&)params {
  [_loader loadURLWithParams:params];
}

- (OverscrollActionsController*)overscrollActionsController {
  return _overscrollActionsController;
}

- (void)setOverscrollActionsController:
    (OverscrollActionsController*)controller {
  _overscrollActionsController = controller;
  [[self scrollView] setDelegate:controller];
}

#pragma mark -
#pragma mark CRWNativeContent implementation

- (void)willBeDismissed {
  // Invalidate the _overscrollActionsController but let the animation finish.
  [_overscrollActionsController scheduleInvalidate];
}

- (void)close {
  [_overscrollActionsController invalidate];
}

- (void)willUpdateSnapshot {
  [_overscrollActionsController clear];
}

- (const GURL&)url {
  return _URL;
}

- (UIView*)view {
  CHECK(_webUsageEnabled) << "Tried to create a web view when web usage was"
                          << " disabled!";
  return [_staticHTMLViewController webView];
}

- (void)setDelegate:(id<CRWNativeContentDelegate>)delegate {
  [_staticHTMLViewController setDelegate:delegate];
}

- (NSString*)title {
  return [_staticHTMLViewController title];
}

- (void)reload {
  [_staticHTMLViewController reload];
}

- (BOOL)isViewAlive {
  return [_staticHTMLViewController isViewAlive];
}

- (void)executeJavaScript:(NSString*)script
        completionHandler:(web::JavaScriptResultBlock)handler {
  [_staticHTMLViewController executeJavaScript:script
                             completionHandler:handler];
}

- (void)setScrollEnabled:(BOOL)enabled {
  [_staticHTMLViewController setScrollEnabled:enabled];
}

- (void)setWebUsageEnabled:(BOOL)webUsageEnabled {
  if (_webUsageEnabled == webUsageEnabled) {
    return;
  }
  _webUsageEnabled = webUsageEnabled;
  if (!_webUsageEnabled) {
    [_overscrollActionsController invalidate];
    [[self scrollView] setDelegate:nil];
    _staticHTMLViewController = nil;
  }
}

- (UIScrollView*)scrollView {
  return [_staticHTMLViewController scrollView];
}

@end
