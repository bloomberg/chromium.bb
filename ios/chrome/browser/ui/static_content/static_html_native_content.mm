// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/static_content/static_html_native_content.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#include "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/web/public/referrer.h"

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
  base::scoped_nsobject<StaticHtmlViewController> _staticHTMLViewController;
  // Responsible for loading a particular URL.
  id<UrlLoader> _loader;  // weak
  // The controller handling the overscroll actions.
  base::scoped_nsobject<OverscrollActionsController>
      _overscrollActionsController;
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
    _staticHTMLViewController.reset([HTMLViewController retain]);
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
  base::scoped_nsobject<StaticHtmlViewController> HTMLViewController(
      [[StaticHtmlViewController alloc] initWithResource:resourcePath
                                            browserState:browserState]);
  return [self initWithLoader:loader
      staticHTMLViewController:HTMLViewController
                           URL:URL];
}

- (void)dealloc {
  [[self scrollView] setDelegate:nil];
  [super dealloc];
}

- (void)loadURL:(const GURL&)URL
             referrer:(const web::Referrer&)referrer
           transition:(ui::PageTransition)transition
    rendererInitiated:(BOOL)rendererInitiated {
  [_loader loadURL:URL
               referrer:referrer
             transition:transition
      rendererInitiated:rendererInitiated];
}

- (OverscrollActionsController*)overscrollActionsController {
  return _overscrollActionsController.get();
}

- (void)setOverscrollActionsController:
    (OverscrollActionsController*)controller {
  _overscrollActionsController.reset([controller retain]);
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

- (void)handleLowMemory {
  [_staticHTMLViewController handleLowMemory];
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
    _staticHTMLViewController.reset();
  }
}

- (UIScrollView*)scrollView {
  return [_staticHTMLViewController scrollView];
}

@end
