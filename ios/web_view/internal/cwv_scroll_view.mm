// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_scroll_view.h"

#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web_view/internal/cwv_scroll_view_internal.h"
#import "ios/web_view/public/cwv_scroll_view_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVScrollView ()<CRWWebViewScrollViewProxyObserver>

// For KVO compliance, redefines the property as readwrite and calls its setter
// when the value changes, instead of defining a getter which directly calls
// _proxy.contentSize.
@property(nonatomic, readwrite) CGSize contentSize;

@end

@implementation CWVScrollView

@synthesize contentSize = _contentSize;
@synthesize delegate = _delegate;
@synthesize proxy = _proxy;

- (void)setProxy:(nullable CRWWebViewScrollViewProxy*)proxy {
  [_proxy removeObserver:self];
  _proxy = proxy;
  self.contentSize = _proxy.contentSize;
  [_proxy addObserver:self];
}

- (CGPoint)contentOffset {
  return _proxy.contentOffset;
}

- (void)setContentOffset:(CGPoint)contentOffset {
  _proxy.contentOffset = contentOffset;
}

- (CGRect)bounds {
  return {_proxy.contentOffset, _proxy.frame.size};
}

- (BOOL)isDragging {
  return _proxy.dragging;
}

- (UIEdgeInsets)contentInset {
  return _proxy.contentInset;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  _proxy.contentInset = contentInset;
}

- (void)addGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_proxy addGestureRecognizer:gestureRecognizer];
}

#pragma mark - NSObject

- (void)dealloc {
  // Removes |self| from |_proxy|'s observers. Otherwise |_proxy| will keep a
  // dangling pointer to |self| and cause SEGV later.
  [_proxy removeObserver:self];
}

#pragma mark - CRWWebViewScrollViewObserver

- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  SEL selector = @selector(webViewScrollViewWillBeginDragging:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate scrollViewWillBeginDragging:self];
  }
}
- (void)webViewScrollViewWillEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                            withVelocity:(CGPoint)velocity
                     targetContentOffset:(inout CGPoint*)targetContentOffset {
  SEL selector = @selector
      (webViewScrollViewWillEndDragging:withVelocity:targetContentOffset:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate scrollViewWillEndDragging:self
                            withVelocity:velocity
                     targetContentOffset:targetContentOffset];
  }
}

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  SEL selector = @selector(scrollViewDidScroll:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate scrollViewDidScroll:self];
  }
}

- (void)webViewScrollViewDidEndDecelerating:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  SEL selector = @selector(webViewScrollViewDidEndDecelerating:);
  if ([_delegate respondsToSelector:selector]) {
    [_delegate scrollViewDidEndDecelerating:self];
  }
}

- (void)webViewScrollViewDidResetContentSize:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  self.contentSize = _proxy.contentSize;
}

@end
