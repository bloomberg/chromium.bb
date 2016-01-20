// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"

#include "base/auto_reset.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/ios/weak_nsobject.h"
#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"

@interface CRWWebViewScrollViewProxy () {
  base::WeakNSObject<UIScrollView> _scrollView;
  base::scoped_nsobject<id> _observers;
  // When |_ignoreScroll| is set to YES, do not pass on -scrollViewDidScroll
  // calls to observers.  This is used by -setContentInsetFast, which needs to
  // update and reset the contentOffset to force a fast update.  These updates
  // should be a no-op for the contentOffset, so the callbacks can be ignored.
  BOOL _ignoreScroll;
  // The number of calls through the proxy API in the current stack.
  NSUInteger _proxyCallCount;
}

// Returns the key paths that need to be observed for UIScrollView.
+ (NSArray*)scrollViewObserverKeyPaths;

// Adds and removes |self| as an observer for |scrollView| with key paths
// returned by |+scrollViewObserverKeyPaths|.
- (void)startObservingScrollView:(UIScrollView*)scrollView;
- (void)stopObservingScrollView:(UIScrollView*)scrollView;

@end

@implementation CRWWebViewScrollViewProxy

- (instancetype)init {
  self = [super init];
  if (self) {
    Protocol* protocol = @protocol(CRWWebViewScrollViewProxyObserver);
    _observers.reset(
        [[CRBProtocolObservers observersWithProtocol:protocol] retain]);
  }
  return self;
}

- (void)dealloc {
  [self stopObservingScrollView:_scrollView];
  [super dealloc];
}

- (void)addGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_scrollView addGestureRecognizer:gestureRecognizer];
}

- (void)removeGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_scrollView removeGestureRecognizer:gestureRecognizer];
}

- (void)addObserver:(id<CRWWebViewScrollViewProxyObserver>)observer {
  [_observers addObserver:observer];
}

- (void)removeObserver:(id<CRWWebViewScrollViewProxyObserver>)observer {
  [_observers removeObserver:observer];
}

- (void)setScrollView:(UIScrollView*)scrollView {
  if (_scrollView == scrollView)
    return;
  [_scrollView setDelegate:nil];
  [self stopObservingScrollView:_scrollView];
  DCHECK(!scrollView.delegate);
  scrollView.delegate = self;
  [self startObservingScrollView:scrollView];
  _scrollView.reset(scrollView);
  [_observers webViewScrollViewProxyDidSetScrollView:self];
}

- (CGRect)frame {
  return _scrollView ? [_scrollView frame] : CGRectZero;
}

- (BOOL)isScrollEnabled {
  return [_scrollView isScrollEnabled];
}

- (void)setScrollEnabled:(BOOL)scrollEnabled {
  base::AutoReset<NSUInteger> autoReset(&_proxyCallCount, _proxyCallCount + 1);
  [_scrollView setScrollEnabled:scrollEnabled];
}

- (BOOL)bounces {
  return [_scrollView bounces];
}

- (void)setBounces:(BOOL)bounces {
  base::AutoReset<NSUInteger> autoReset(&_proxyCallCount, _proxyCallCount + 1);
  [_scrollView setBounces:bounces];
}

- (BOOL)isZooming {
  return [_scrollView isZooming];
}

- (CGFloat)zoomScale {
  return [_scrollView zoomScale];
}

- (void)setContentOffset:(CGPoint)contentOffset {
  base::AutoReset<NSUInteger> autoReset(&_proxyCallCount, _proxyCallCount + 1);
  [_scrollView setContentOffset:contentOffset];
}

- (CGPoint)contentOffset {
  return _scrollView ? [_scrollView contentOffset] : CGPointZero;
}

- (void)setContentInsetFast:(UIEdgeInsets)contentInset {
  base::AutoReset<NSUInteger> autoReset(&_proxyCallCount, _proxyCallCount + 1);
  if (!_scrollView)
    return;

  // The method -scrollViewSetContentInsetImpl below is bypassing UIWebView's
  // subclassed UIScrollView implemention of setContentOffset.  UIKIt's
  // implementation calls the internal method |_updateViewSettings| after
  // updating the contentInsets.  This ensures things like absolute positions
  // are correctly updated.  The problem is |_updateViewSettings| does lots of
  // other things and is very very slow.  The workaround below simply sets the
  // scrollView's content insets directly, and then fiddles with the
  // contentOffset below to correct the absolute positioning of elements.
  static void (*scrollViewSetContentInsetImpl)(id, SEL, UIEdgeInsets);
  static SEL setContentInset;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
      setContentInset = @selector(setContentInset:);
      scrollViewSetContentInsetImpl =
          (void (*)(id, SEL, UIEdgeInsets))class_getMethodImplementation(
              [UIScrollView class], setContentInset);
  });
  scrollViewSetContentInsetImpl(_scrollView, setContentInset, contentInset);

  // Change and then reset the contentOffset to force the view into updating the
  // absolute position of elements and content frame. Updating the
  // contentOffset will cause the -scrollViewDidScroll callback to fire.
  // Because we are eventually setting the contentOffset back to it's original
  // position, we can ignore these calls.
  base::AutoReset<BOOL> ignoreScrollAutoReset(&_ignoreScroll, YES);
  CGPoint contentOffset = [_scrollView contentOffset];
  _scrollView.get().contentOffset =
      CGPointMake(contentOffset.x, contentOffset.y + 1);
  _scrollView.get().contentOffset = contentOffset;
}

- (void)setContentInset:(UIEdgeInsets)contentInset {
  base::AutoReset<NSUInteger> autoReset(&_proxyCallCount, _proxyCallCount + 1);
  [_scrollView setContentInset:contentInset];
}

- (UIEdgeInsets)contentInset {
  return _scrollView ? [_scrollView contentInset] : UIEdgeInsetsZero;
}

- (void)setScrollIndicatorInsets:(UIEdgeInsets)scrollIndicatorInsets {
  base::AutoReset<NSUInteger> autoReset(&_proxyCallCount, _proxyCallCount + 1);
  [_scrollView setScrollIndicatorInsets:scrollIndicatorInsets];
}

- (UIEdgeInsets)scrollIndicatorInsets {
  return _scrollView ? [_scrollView scrollIndicatorInsets] : UIEdgeInsetsZero;
}

- (void)setContentSize:(CGSize)contentSize {
  base::AutoReset<NSUInteger> autoReset(&_proxyCallCount, _proxyCallCount + 1);
  [_scrollView setContentSize:contentSize];
}

- (CGSize)contentSize {
  return _scrollView ? [_scrollView contentSize] : CGSizeZero;
}

- (void)setContentOffset:(CGPoint)contentOffset animated:(BOOL)animated {
  base::AutoReset<NSUInteger> autoReset(&_proxyCallCount, _proxyCallCount + 1);
  [_scrollView setContentOffset:contentOffset animated:animated];
}

- (UIPanGestureRecognizer*)panGestureRecognizer {
  return [_scrollView panGestureRecognizer];
}

- (NSArray*)gestureRecognizers {
  return [_scrollView gestureRecognizers];
}

- (BOOL)isUpdatingThroughProxy {
  return _proxyCallCount > 0;
}

#pragma mark -
#pragma mark UIScrollViewDelegate callbacks

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  if (!_ignoreScroll) {
    [_observers webViewScrollViewDidScroll:self];
  }
}

- (void)scrollViewWillBeginDragging:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewWillBeginDragging:self];

  // TODO(crbug.com/555723) Remove this once the fix to
  // https://bugs.webkit.org/show_bug.cgi?id=148086 makes it's way in to iOS.
  scrollView.decelerationRate = UIScrollViewDecelerationRateNormal;
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewWillEndDragging:self
                                  withVelocity:velocity
                           targetContentOffset:targetContentOffset];
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidEndDragging:self willDecelerate:decelerate];
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidEndDecelerating:self];
}

- (void)scrollViewDidEndScrollingAnimation:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidEndScrollingAnimation:self];
}

- (BOOL)scrollViewShouldScrollToTop:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  __block BOOL shouldScrollToTop = YES;
  [_observers executeOnObservers:^(id observer) {
      if ([observer respondsToSelector:@selector(
          webViewScrollViewShouldScrollToTop:)]) {
        shouldScrollToTop = shouldScrollToTop &&
            [observer webViewScrollViewShouldScrollToTop:self];
      }
  }];
  return shouldScrollToTop;
}

- (void)scrollViewDidZoom:(UIScrollView*)scrollView {
  DCHECK_EQ(_scrollView, scrollView);
  [_observers webViewScrollViewDidZoom:self];
}

#pragma mark -

+ (NSArray*)scrollViewObserverKeyPaths {
  return @[ @"contentSize" ];
}

- (void)startObservingScrollView:(UIScrollView*)scrollView {
  for (NSString* keyPath in [[self class] scrollViewObserverKeyPaths])
    [scrollView addObserver:self forKeyPath:keyPath options:0 context:nil];
}

- (void)stopObservingScrollView:(UIScrollView*)scrollView {
  for (NSString* keyPath in [[self class] scrollViewObserverKeyPaths])
    [scrollView removeObserver:self forKeyPath:keyPath];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK_EQ(object, _scrollView.get());
  if ([keyPath isEqualToString:@"contentSize"])
    [_observers webViewScrollViewDidResetContentSize:self];
}

@end
