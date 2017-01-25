// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CRIWV_WEB_VIEW_DELEGATE_H_
#define IOS_WEB_VIEW_PUBLIC_CRIWV_WEB_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

@protocol CRIWVTranslateDelegate;
@protocol CRIWVWebView;

typedef NS_OPTIONS(NSUInteger, CRIWVWebViewUpdateType) {
  CRIWVWebViewUpdateTypeProgress = 1 << 0,
  CRIWVWebViewUpdateTypeTitle = 1 << 1,
  CRIWVWebViewUpdateTypeURL = 1 << 2
};

// Delegate protocol for CRIWVWebViews.  Allows embedders to customize web view
// behavior and receive updates on page load progress.
@protocol CRIWVWebViewDelegate<NSObject>

@optional

- (void)webView:(id<CRIWVWebView>)webView
    didFinishLoadingWithURL:(NSURL*)url
                loadSuccess:(BOOL)loadSuccess;

- (void)webView:(id<CRIWVWebView>)webView
    didUpdateWithChanges:(CRIWVWebViewUpdateType)changes;

- (id<CRIWVTranslateDelegate>)translateDelegate;

@end

#endif  // IOS_WEB_VIEW_PUBLIC_CRIWV_WEB_VIEW_DELEGATE_H_
