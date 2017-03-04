// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/static_content/static_html_view_controller.h"

#import <WebKit/WebKit.h>

#include <stdlib.h>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/ui/crw_context_menu_delegate.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_view_creation_util.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"

@implementation IdrHtmlGenerator
- (id)initWithResourceId:(int)resourceId encoding:(NSStringEncoding)encoding {
  if ((self = [super init])) {
    resourceId_ = resourceId;
    encoding_ = encoding;
  }
  return self;
}

- (id)initWithResourceId:(int)resourceId {
  return [self initWithResourceId:resourceId encoding:NSUTF8StringEncoding];
}

- (void)generateHtml:(HtmlCallback)callback {
  base::StringPiece html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(resourceId_));
  base::scoped_nsobject<NSString> result([[NSString alloc]
      initWithBytes:html.data()
             length:html.size()
           encoding:encoding_]);
  callback(result);
}
@end

@interface StaticHtmlViewController ()<CRWContextMenuDelegate,
                                       WKNavigationDelegate> {
 @private
  // The referrer that will be passed when navigating from this page.
  web::Referrer referrer_;

  // The URL that will be passed to the web page when loading.
  // If the page is displaying a local HTML file, it contains the file URL to
  // the file.
  // If the page is a generated HTML, it contains a random resource URL.
  base::scoped_nsobject<NSURL> resourceUrl_;

  // If the view is displaying a local file, contains the URL of the root
  // directory containing all resources needed by the page.
  // The web view will get access to this page.
  base::scoped_nsobject<NSURL> resourcesRootDirectory_;

  // If the view is displaying a resources, contains the name of the resource.
  base::scoped_nsobject<NSString> resource_;

  // If the view displayes a generated HTML, contains the |HtmlGenerator| to
  // generate it.
  base::scoped_nsprotocol<id<HtmlGenerator>> generator_;

  // Browser state associated with the view controller, used to create the
  // WKWebView.
  web::BrowserState* browserState_;  // Weak.

  // The web view that is used to display the content.
  base::scoped_nsobject<WKWebView> webView_;

  // The delegate of the native content.
  base::WeakNSProtocol<id<CRWNativeContentDelegate>> delegate_;  // weak

  // The loader to navigate from the page.
  base::WeakNSProtocol<id<UrlLoader>> loader_;  // weak
}

// Returns the URL of the static page to display.
- (NSURL*)resourceURL;
// Ensures that webView_ has been created, creating it if necessary.
- (void)ensureWebViewCreated;
// Determines if the page load should begin based on the current |resourceURL|
// and if the request is issued by the main frame (|fromMainFrame|).
- (BOOL)shouldStartLoadWithRequest:(NSURLRequest*)request
                     fromMainFrame:(BOOL)fromMainFrame;
@end

@implementation StaticHtmlViewController

- (instancetype)initWithResource:(NSString*)resource
                    browserState:(web::BrowserState*)browserState {
  DCHECK(resource);
  DCHECK(browserState);
  if ((self = [super init])) {
    resource_.reset([resource copy]);
    browserState_ = browserState;
  }
  return self;
}

- (instancetype)initWithGenerator:(id<HtmlGenerator>)generator
                     browserState:(web::BrowserState*)browserState {
  DCHECK(generator);
  DCHECK(browserState);
  if ((self = [super init])) {
    generator_.reset([generator retain]);
    browserState_ = browserState;
  }
  return self;
}

- (instancetype)initWithFileURL:(const GURL&)URL
        allowingReadAccessToURL:(const GURL&)resourcesRoot
                   browserState:(web::BrowserState*)browserState {
  DCHECK(URL.is_valid());
  DCHECK(URL.SchemeIsFile());
  DCHECK(browserState);
  if ((self = [super init])) {
    resourceUrl_.reset([net::NSURLWithGURL(URL) retain]);
    resourcesRootDirectory_.reset([net::NSURLWithGURL(resourcesRoot) retain]);
    browserState_ = browserState;
  }
  return self;
}

- (void)dealloc {
  [self removeWebViewObservers];
  [super dealloc];
}

- (void)removeWebViewObservers {
  [webView_ removeObserver:self forKeyPath:@"title"];
}

- (void)setLoader:(id<UrlLoader>)loader
         referrer:(const web::Referrer&)referrer {
  loader_.reset(loader);
  referrer_ = referrer;
}

- (void)executeJavaScript:(NSString*)script
        completionHandler:(web::JavaScriptResultBlock)handler {
  [webView_ evaluateJavaScript:script completionHandler:handler];
}

- (UIScrollView*)scrollView {
  return [[self webView] scrollView];
}

- (WKWebView*)webView {
  [self ensureWebViewCreated];
  return webView_;
}

- (void)handleLowMemory {
  [self removeWebViewObservers];
  webView_.reset();
}

- (BOOL)isViewAlive {
  return webView_.get() != nil;
}

- (NSString*)title {
  return [[self webView] title];
}

- (void)reload {
  if (!generator_) {
    [webView_ reload];
  } else {
    NSURL* resourceURL = [self resourceURL];
    [generator_ generateHtml:^(NSString* HTML) {
      [webView_ loadHTMLString:HTML baseURL:resourceURL];
    }];
  }
}

- (void)triggerPendingLoad {
  // Ensure that the web view is created, which triggers loading.
  [self ensureWebViewCreated];
}

- (void)setDelegate:(id<CRWNativeContentDelegate>)delegate {
  delegate_.reset(delegate);
}

- (void)setScrollEnabled:(BOOL)enabled {
  [[self scrollView] setScrollEnabled:enabled];
}

#pragma mark -
#pragma mark WKNavigationDelegate implementation

- (void)webView:(WKWebView*)webView
    decidePolicyForNavigationAction:(WKNavigationAction*)navigationAction
                    decisionHandler:
                        (void (^)(WKNavigationActionPolicy))decisionHandler {
  decisionHandler(
      [self
          shouldStartLoadWithRequest:navigationAction.request
                       fromMainFrame:[navigationAction.targetFrame isMainFrame]]
          ? WKNavigationActionPolicyAllow
          : WKNavigationActionPolicyCancel);
}

#pragma mark -
#pragma mark CRWContextMenuDelegate implementation

- (BOOL)webView:(WKWebView*)webView
    handleContextMenu:(const web::ContextMenuParams&)params {
  if ([delegate_
          respondsToSelector:@selector(nativeContent:handleContextMenu:)]) {
    return [delegate_ nativeContent:self handleContextMenu:params];
  }
  return NO;
}

#pragma mark -
#pragma mark KVO callback

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK([keyPath isEqualToString:@"title"]);
  if ([delegate_ respondsToSelector:@selector(nativeContent:titleDidChange:)]) {
    // WKWebView's |title| changes to nil when its web process crashes.
    if ([webView_ title])
      [delegate_ nativeContent:self titleDidChange:[webView_ title]];
  }
}

#pragma mark -
#pragma mark Private

- (BOOL)shouldStartLoadWithRequest:(NSURLRequest*)request
                     fromMainFrame:(BOOL)fromMainFrame {
  // Only allow displaying the URL which correspond to the authorized resource.
  if ([[request URL] isEqual:[self resourceURL]])
    return YES;

  // All other navigation URLs will be loaded by our UrlLoader if one exists and
  // if they are issued by the main frame.
  if (loader_ && fromMainFrame) {
    dispatch_async(dispatch_get_main_queue(), ^{
      [loader_ loadURL:net::GURLWithNSURL([request URL])
                   referrer:referrer_
                 transition:ui::PAGE_TRANSITION_LINK
          rendererInitiated:YES];
    });
  }
  return NO;
}

- (NSURL*)resourceURL {
  if (resourceUrl_)
    return resourceUrl_.get();

  DCHECK(resource_ || generator_);
  NSString* path = nil;
  if (resource_) {
    NSBundle* bundle = base::mac::FrameworkBundle();
    NSString* bundlePath = [bundle bundlePath];
    path = [bundlePath stringByAppendingPathComponent:resource_.get()];
  } else {
    // Generate a random resource URL to whitelist the load in
    // |webView:shouldStartLoadWithRequest:navigationType:| method.
    path = [NSString stringWithFormat:@"/whitelist%u%u%u%u", arc4random(),
                                      arc4random(), arc4random(), arc4random()];
  }
  DCHECK(path);
  // Necessary because the |fileURLWithPath:| method adds a localhost in the
  // URL, and this prevents the URL from being comparable with the ones that
  // UIWebView uses when calling the delegate.
  base::scoped_nsobject<NSURLComponents> components(
      [[NSURLComponents alloc] init]);
  [components setScheme:@"file"];
  [components setHost:@""];
  [components setPath:path];
  resourceUrl_.reset([[components URL] retain]);
  resourcesRootDirectory_.reset([resourceUrl_ copy]);
  return resourceUrl_;
}

- (void)ensureWebViewCreated {
  if (!webView_) {
    WKWebView* webView = web::BuildWKWebViewWithCustomContextMenu(
        CGRectZero, browserState_, self);
    [webView addObserver:self forKeyPath:@"title" options:0 context:nullptr];
    [webView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                 UIViewAutoresizingFlexibleHeight];
    [self loadWebViewContents:webView];
    [webView setNavigationDelegate:self];
    webView_.reset([webView retain]);
  }
}

- (void)loadWebViewContents:(WKWebView*)webView {
  if (!generator_) {
    [webView loadFileURL:[self resourceURL]
        allowingReadAccessToURL:resourcesRootDirectory_];
  } else {
    NSURL* resourceURL = [self resourceURL];
    [generator_ generateHtml:^(NSString* HTML) {
      [webView loadHTMLString:HTML baseURL:resourceURL];
    }];
  }
}

@end
