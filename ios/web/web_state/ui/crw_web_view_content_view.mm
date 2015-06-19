// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

@interface CRWWebViewContentView () {
  // The web view being shown.
  base::scoped_nsobject<UIView> _webView;
  // The web view's scroll view.
  base::scoped_nsobject<UIScrollView> _scrollView;
}

@end

@implementation CRWWebViewContentView

- (instancetype)initWithWebView:(UIView*)webView
                     scrollView:(UIScrollView*)scrollView {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(webView);
    DCHECK(scrollView);
    DCHECK([scrollView isDescendantOfView:webView]);
    _webView.reset([webView retain]);
    _scrollView.reset([scrollView retain]);
    [self addSubview:_webView];
  }
  return self;
}

#pragma mark Accessors

- (UIScrollView*)scrollView {
  return _scrollView.get();
}

- (UIView*)webView {
  return _webView.get();
}

#pragma mark Layout

- (void)layoutSubviews {
  [super layoutSubviews];
  self.webView.frame = self.bounds;
}

- (BOOL)isViewAlive {
  return YES;
}

@end
