// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_view_content_view.h"

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"

@interface TestWebViewContentView () {
  base::scoped_nsprotocol<id> _mockWebView;
  base::scoped_nsprotocol<id> _mockScrollView;
}

@end

@implementation TestWebViewContentView

- (instancetype)initWithMockWebView:(id)webView scrollView:(id)scrollView {
  self = [super initForTesting];
  if (self) {
    DCHECK(webView);
    DCHECK(scrollView);
    _mockWebView.reset([webView retain]);
    _mockScrollView.reset([scrollView retain]);
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)decoder {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame {
  NOTREACHED();
  return nil;
}

#pragma mark Accessors

- (UIScrollView*)scrollView {
  return static_cast<UIScrollView*>(_mockScrollView.get());
}

- (UIView*)webView {
  return static_cast<UIView*>(_mockWebView.get());
}

@end
