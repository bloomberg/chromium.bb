// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

namespace {

// Background color RGB values for the content view which is displayed when the
// |_webView| is offset from the screen due to user interaction. Displaying this
// background color is handled by UIWebView but not WKWebView, so it needs to be
// set in CRWWebViewContentView to support both. The color value matches that
// used by UIWebView.
const CGFloat kBackgroundRGBComponents[] = {0.75f, 0.74f, 0.76f};

}  // namespace

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
  }
  return self;
}

- (instancetype)initForTesting {
  return [super initWithFrame:CGRectZero];
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

- (void)willMoveToSuperview:(nullable UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];
  self.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  [self addSubview:_webView];
  self.backgroundColor = [UIColor colorWithRed:kBackgroundRGBComponents[0]
                                         green:kBackgroundRGBComponents[1]
                                          blue:kBackgroundRGBComponents[2]
                                         alpha:1.0];
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
