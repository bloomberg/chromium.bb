// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_web_view_proxy_impl.h"

#import "base/mac/scoped_nsobject.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the first responder in the subviews of |view|, or nil if no view in
// the subtree is the first responder.
UIView* GetFirstResponderSubview(UIView* view) {
  if ([view isFirstResponder])
    return view;

  for (UIView* subview in [view subviews]) {
    UIView* firstResponder = GetFirstResponderSubview(subview);
    if (firstResponder)
      return firstResponder;
  }

  return nil;
}

}  // namespace

@interface CRWWebViewScrollViewProxy (ContentInsetsAlgebra)
// Adds the given insets to the current content insets and scroll indicator
// insets of the receiver.
- (void)cr_addInsets:(UIEdgeInsets)insets;
// Removes the given insets to the current content insets and scroll indicator
// insets of the receiver.
- (void)cr_removeInsets:(UIEdgeInsets)insets;
@end

@implementation CRWWebViewScrollViewProxy (ContentInsetsAlgebra)

- (void)cr_addInsets:(UIEdgeInsets)insets {
  if (UIEdgeInsetsEqualToEdgeInsets(insets, UIEdgeInsetsZero))
    return;

  UIEdgeInsets currentInsets = [self contentInset];
  currentInsets.top += insets.top;
  currentInsets.left += insets.left;
  currentInsets.bottom += insets.bottom;
  currentInsets.right += insets.right;
  [self setContentInset:currentInsets];
  [self setScrollIndicatorInsets:currentInsets];
}

- (void)cr_removeInsets:(UIEdgeInsets)insets {
  UIEdgeInsets negativeInsets = UIEdgeInsetsZero;
  negativeInsets.top = -insets.top;
  negativeInsets.left = -insets.left;
  negativeInsets.bottom = -insets.bottom;
  negativeInsets.right = -insets.right;
  [self cr_addInsets:negativeInsets];
}

@end

@implementation CRWWebViewProxyImpl {
  __weak CRWWebController* _webController;
  base::scoped_nsobject<NSMutableDictionary> _registeredInsets;
  // The WebViewScrollViewProxy is a wrapper around the web view's
  // UIScrollView to give components access in a limited and controlled manner.
  base::scoped_nsobject<CRWWebViewScrollViewProxy> _contentViewScrollViewProxy;
}
@synthesize contentView = _contentView;

- (instancetype)initWithWebController:(CRWWebController*)webController {
  self = [super init];
  if (self) {
    DCHECK(webController);
    _registeredInsets.reset([[NSMutableDictionary alloc] init]);
    _webController = webController;
    _contentViewScrollViewProxy.reset([[CRWWebViewScrollViewProxy alloc] init]);
  }
  return self;
}

- (CRWWebViewScrollViewProxy*)scrollViewProxy {
  return _contentViewScrollViewProxy.get();
}

- (CGRect)bounds {
  return [_contentView bounds];
}

- (CGRect)frame {
  return [_contentView frame];
}

- (CGFloat)topContentPadding {
  return [_contentView topContentPadding];
}

- (void)setTopContentPadding:(CGFloat)newTopContentPadding {
  [_contentView setTopContentPadding:newTopContentPadding];
}

- (NSArray*)gestureRecognizers {
  return [_contentView gestureRecognizers];
}

- (void)addGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_contentView addGestureRecognizer:gestureRecognizer];
}

- (void)removeGestureRecognizer:(UIGestureRecognizer*)gestureRecognizer {
  [_contentView removeGestureRecognizer:gestureRecognizer];
}

- (BOOL)shouldUseInsetForTopPadding {
  SEL shouldUseInsetSelector = @selector(shouldUseInsetForTopPadding);
  return [_contentView respondsToSelector:shouldUseInsetSelector] &&
         [_contentView shouldUseInsetForTopPadding];
}

- (void)setShouldUseInsetForTopPadding:(BOOL)shouldUseInsetForTopPadding {
  if ([_contentView
          respondsToSelector:@selector(setShouldUseInsetForTopPadding:)]) {
    [_contentView setShouldUseInsetForTopPadding:shouldUseInsetForTopPadding];
  }
}

- (void)registerInsets:(UIEdgeInsets)insets forCaller:(id)caller {
  NSValue* callerValue = [NSValue valueWithNonretainedObject:caller];
  if ([_registeredInsets objectForKey:callerValue])
    [self unregisterInsetsForCaller:caller];
  [self.scrollViewProxy cr_addInsets:insets];
  [_registeredInsets setObject:[NSValue valueWithUIEdgeInsets:insets]
                        forKey:callerValue];
}

- (void)unregisterInsetsForCaller:(id)caller {
  NSValue* callerValue = [NSValue valueWithNonretainedObject:caller];
  NSValue* insetsValue = [_registeredInsets objectForKey:callerValue];
  [self.scrollViewProxy cr_removeInsets:[insetsValue UIEdgeInsetsValue]];
  [_registeredInsets removeObjectForKey:callerValue];
}

- (void)setContentView:(CRWContentView*)contentView {
  _contentView = contentView;
  [_contentViewScrollViewProxy setScrollView:contentView.scrollView];
}

- (void)addSubview:(UIView*)view {
  return [_contentView addSubview:view];
}

- (BOOL)hasSearchableTextContent {
  return _contentView != nil && [_webController contentIsHTML];
}

- (UIView*)keyboardAccessory {
  if (!_contentView)
    return nil;
  UIView* firstResponder = GetFirstResponderSubview(_contentView);
  return firstResponder.inputAccessoryView;
}

- (UITextInputAssistantItem*)inputAssistantItem {
  if (!_contentView)
    return nil;
  UIView* firstResponder = GetFirstResponderSubview(_contentView);
  return firstResponder.inputAssistantItem;
}

- (BOOL)becomeFirstResponder {
  return [_contentView becomeFirstResponder];
}

@end
