// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"

#import <WebKit/WebKit.h>

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
  // Backs up property of the same name if |_webView| is a WKWebView.
  CGFloat _topContentPadding;
}

// Changes web view frame to match |self.bounds| and optionally accomodates for
// |_topContentPadding| (iff |_webView| is a WKWebView).
- (void)updateWebViewFrame;

@end

@implementation CRWWebViewContentView

@synthesize webViewType = _webViewType;

- (instancetype)initWithWebView:(UIView*)webView
                     scrollView:(UIScrollView*)scrollView {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(webView);
    DCHECK(scrollView);
    DCHECK([scrollView isDescendantOfView:webView]);
    _webView.reset([webView retain]);
    _scrollView.reset([scrollView retain]);
    _webViewType = [webView isKindOfClass:[WKWebView class]]
                       ? web::WK_WEB_VIEW_TYPE
                       : web::UI_WEB_VIEW_TYPE;
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

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  if (self.superview) {
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self addSubview:_webView];
    self.backgroundColor = [UIColor colorWithRed:kBackgroundRGBComponents[0]
                                           green:kBackgroundRGBComponents[1]
                                            blue:kBackgroundRGBComponents[2]
                                           alpha:1.0];
  }
}

- (BOOL)becomeFirstResponder {
  return [_webView becomeFirstResponder];
}

- (void)setFrame:(CGRect)frame {
  if (CGRectEqualToRect(self.frame, frame))
    return;
  [super setFrame:frame];
  [self updateWebViewFrame];
}

- (void)setBounds:(CGRect)bounds {
  if (CGRectEqualToRect(self.bounds, bounds))
    return;
  [super setBounds:bounds];
  [self updateWebViewFrame];
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
  [self updateWebViewFrame];
}

- (BOOL)isViewAlive {
  return YES;
}

- (CGFloat)topContentPadding {
  return self.webViewType == web::WK_WEB_VIEW_TYPE
             ? _topContentPadding
             : [_scrollView contentInset].top;
}

- (void)setTopContentPadding:(CGFloat)newTopPadding {
  if (self.webViewType == web::WK_WEB_VIEW_TYPE) {
    if (_topContentPadding != newTopPadding) {
      _topContentPadding = newTopPadding;
      // Update web view frame immediately to make |topContentPadding|
      // animatable.
      [self updateWebViewFrame];
    }
  } else {
    UIEdgeInsets inset = [_scrollView contentInset];
    inset.top = newTopPadding;
    [_scrollView setContentInset:inset];
  }
}

#pragma mark Private methods

- (void)updateWebViewFrame {
  CGRect webViewFrame = self.bounds;
  webViewFrame.size.height -= _topContentPadding;
  webViewFrame.origin.y += _topContentPadding;

  self.webView.frame = webViewFrame;
}

@end
