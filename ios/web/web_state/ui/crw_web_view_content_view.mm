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
    // The frame needs to be set immediately after the web view is added
    // as a subview. The change in the frame triggers drawing operations and
    // if not done after it's added as a subview, the web view exhibits
    // strange behavior where clicks from certain web sites are not triggered.
    // The actual value of the frame doesn't matter as long as it's not
    // CGRectZero.  The CRWWebViewContentView's frame will be reset to a correct
    // value in a subsequent layout pass.
    // TODO(crbug.com/577793): This is an undocumented and not-well-understood
    // workaround for this issue.
    const CGRect kDummyRect = CGRectMake(10, 20, 30, 50);
    self.frame = kDummyRect;
  }
}

- (BOOL)becomeFirstResponder {
  return [_webView becomeFirstResponder];
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
