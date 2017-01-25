// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_VIEW_PUBLIC_CRIWV_WEB_VIEW_H_
#define IOS_WEB_VIEW_PUBLIC_CRIWV_WEB_VIEW_H_

#import <UIKit/UIKit.h>

@protocol CRIWVWebViewDelegate;

// Primary objective-c interface for web/.  Just like a WKWebView, but better.
// Concrete instances can be created through CRIWV.
@protocol CRIWVWebView<NSObject>

// The view used to display web content.
@property(nonatomic, readonly) UIView* view;

// This web view's delegate.
@property(nonatomic, readwrite, assign) id<CRIWVWebViewDelegate> delegate;

// Whether or not this web view can go backwards or forwards.
@property(nonatomic, readonly) BOOL canGoBack;
@property(nonatomic, readonly) BOOL canGoForward;

// Whether or not this web view is loading a page.
@property(nonatomic, readonly) BOOL isLoading;

// The current URL, for display to the user.
@property(nonatomic, readonly) NSURL* visibleURL;

// The current page title.
@property(nonatomic, readonly) NSString* pageTitle;

// The current load progress, as a fraction between 0 and 1.  This value is
// undefined if the web view is not currently loading.
@property(nonatomic, readonly) CGFloat loadProgress;

// Navigates backwards or forwards by one page.  Does nothing if the
// corresponding |canGoBack| or |canGoForward| method returns NO.
- (void)goBack;
- (void)goForward;

// Reloads the current page.
- (void)reload;

// Stops loading the current page.
- (void)stopLoading;

// Loads the given |url| in this web view.
- (void)loadURL:(NSURL*)url;

// Evaluates a JavaScript string.
// The completion handler is invoked when script evaluation completes.
- (void)evaluateJavaScript:(NSString*)javaScriptString
         completionHandler:(void (^)(id, NSError*))completionHandler;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CRIWV_WEB_VIEW_H_
