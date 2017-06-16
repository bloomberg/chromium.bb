// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_H_

#import <UIKit/UIKit.h>

#import "cwv_export.h"

@class CWVScrollView;
@class CWVTranslationController;
@class CWVWebViewConfiguration;
@protocol CWVUIDelegate;
@protocol CWVNavigationDelegate;

// A web view component (like WKWebView) which uses iOS Chromium's web view
// implementation.
//
// In addition to WKWebView features, it allows Translate, Find In Page,
// Customizable Context Menus, and maybe more.
//
// Concrete instances can be created through CWV.
CWV_EXPORT
@interface CWVWebView : UIView

// The configuration of the web view.
@property(nonatomic, readonly) CWVWebViewConfiguration* configuration;

// This web view's navigation delegate.
@property(nonatomic, weak) id<CWVNavigationDelegate> navigationDelegate;

// This web view's translation controller.
@property(nonatomic, readonly) CWVTranslationController* translationController;

// This web view's UI delegate
@property(nonatomic, weak) id<CWVUIDelegate> UIDelegate;

// Whether or not this web view can go backwards or forwards. KVO Compliant.
@property(nonatomic, readonly) BOOL canGoBack;
@property(nonatomic, readonly) BOOL canGoForward;

// Whether or not this web view is loading a page. KVO compliant.
@property(nonatomic, readonly, getter=isLoading) BOOL loading;

// The URL displayed in the url bar. KVO Compliant.
@property(nonatomic, readonly) NSURL* visibleURL;

// The URL of the current document. KVO Compliant.
@property(nonatomic, readonly) NSURL* lastCommittedURL;

// The current page title. KVO compliant.
@property(nonatomic, readonly, copy) NSString* title;

// Page loading progress from 0.0 to 1.0. KVO compliant.
//
// It is 0.0 initially before the first navigation starts. After a navigation
// completes, it remains at 1.0 until a new navigation starts, at which point it
// is reset to 0.0.
@property(nonatomic, readonly) double estimatedProgress;

// The scroll view associated with the web view.
@property(nonatomic, readonly) CWVScrollView* scrollView;

// The User Agent product string used to build the full User Agent.
+ (NSString*)userAgentProduct;

// Customizes the User Agent string by inserting |product|. It should be of the
// format "product/1.0". For example:
// "Mozilla/5.0 (iPhone; CPU iPhone OS 10_3 like Mac OS X) AppleWebKit/603.1.30
// (KHTML, like Gecko) <product> Mobile/16D32 Safari/602.1" where <product>
// will be replaced with |product| or empty string if not set.
//
// NOTE: It is recommended to set |product| before initializing any web views.
// Setting |product| is only guaranteed to affect web views which have not yet
// been initialized. However, exisiting web views could also be affected
// depending upon their internal state.
+ (void)setUserAgentProduct:(NSString*)product;

// Use this method to set the necessary credentials used to communicate with
// the Google API for features such as translate. See this link for more info:
// https://support.google.com/googleapi/answer/6158857
// This method must be called before any |CWVWebViews| are instantiated for
// the keys to be used.
+ (void)setGoogleAPIKey:(NSString*)googleAPIKey
               clientID:(NSString*)clientID
           clientSecret:(NSString*)clientSecret;

// |configuration| must not be null
- (instancetype)initWithFrame:(CGRect)frame
                configuration:(CWVWebViewConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Navigates backwards or forwards by one page.  Does nothing if the
// corresponding |canGoBack| or |canGoForward| method returns NO.
- (void)goBack;
- (void)goForward;

// Reloads the current page.
- (void)reload;

// Stops loading the current page.
- (void)stopLoading;

// Loads the given URL request in this web view.
// Unlike WKWebView, this method supports HTTPBody.
- (void)loadRequest:(NSURLRequest*)request;

// Evaluates a JavaScript string.
// The completion handler is invoked when script evaluation completes.
- (void)evaluateJavaScript:(NSString*)javaScriptString
         completionHandler:(void (^)(id, NSError*))completionHandler;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_H_
