// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller_container_view.h"

#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/web/public/web_state/ui/crw_content_view.h"
#import "ios/web/public/web_state/ui/crw_native_content.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#import "ios/web/web_state/crw_web_view_proxy_impl.h"

#pragma mark - CRWToolbarContainerView

// Class that manages the display of toolbars.
@interface CRWToolbarContainerView : UIView {
  // Backing object for |self.toolbars|.
  base::scoped_nsobject<NSMutableArray> _toolbars;
}

// The toolbars currently managed by this view.
@property(nonatomic, retain, readonly) NSMutableArray* toolbars;

// Adds |toolbar| as a subview and bottom aligns to any previously added
// toolbars.
- (void)addToolbar:(UIView*)toolbar;

// Removes |toolbar| from the container view.
- (void)removeToolbar:(UIView*)toolbar;

@end

@implementation CRWToolbarContainerView

#pragma mark Accessors

- (NSMutableArray*)toolbars {
  if (!_toolbars)
    _toolbars.reset([[NSMutableArray alloc] init]);
  return _toolbars.get();
}

#pragma mark Layout

- (void)layoutSubviews {
  [super layoutSubviews];

  // Bottom-align the toolbars.
  CGPoint toolbarOrigin =
      CGPointMake(self.bounds.origin.x, CGRectGetMaxY(self.bounds));
  for (UIView* toolbar in self.toolbars) {
    CGSize toolbarSize = [toolbar sizeThatFits:self.bounds.size];
    toolbarSize.width = self.bounds.size.width;
    toolbarOrigin.y -= toolbarSize.height;
    toolbar.frame = CGRectMake(toolbarOrigin.x, toolbarOrigin.y,
                               toolbarSize.width, toolbarSize.height);
  }
}

- (CGSize)sizeThatFits:(CGSize)size {
  CGSize boundingRectSize = CGSizeMake(size.width, CGFLOAT_MAX);
  CGFloat necessaryHeight = 0.0f;
  for (UIView* toolbar in self.toolbars)
    necessaryHeight += [toolbar sizeThatFits:boundingRectSize].height;
  return CGSizeMake(size.width, necessaryHeight);
}

#pragma mark Toolbars

- (void)addToolbar:(UIView*)toolbar {
  DCHECK(toolbar);
  DCHECK(![self.toolbars containsObject:toolbar]);
  toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.toolbars addObject:toolbar];
  [self addSubview:toolbar];
}

- (void)removeToolbar:(UIView*)toolbar {
  DCHECK(toolbar);
  DCHECK([self.toolbars containsObject:toolbar]);
  [self.toolbars removeObject:toolbar];
  [toolbar removeFromSuperview];
}

@end

#pragma mark - CRWWebControllerContainerView

@interface CRWWebControllerContainerView () {
  // The delegate passed on initialization.
  base::WeakNSProtocol<id<CRWWebControllerContainerViewDelegate>> _delegate;
  // Backing objects for corresponding properties.
  base::scoped_nsobject<CRWWebViewContentView> _webViewContentView;
  base::scoped_nsprotocol<id<CRWNativeContent>> _nativeController;
  base::scoped_nsobject<CRWContentView> _transientContentView;
  base::scoped_nsobject<CRWToolbarContainerView> _toolbarContainerView;
}

// Redefine properties as readwrite.
@property(nonatomic, retain, readwrite)
    CRWWebViewContentView* webViewContentView;
@property(nonatomic, retain, readwrite) id<CRWNativeContent> nativeController;
@property(nonatomic, retain, readwrite) CRWContentView* transientContentView;

// Container view that displays any added toolbars.  It is always the top-most
// subview, and is bottom aligned with the CRWWebControllerContainerView.
@property(nonatomic, retain, readonly)
    CRWToolbarContainerView* toolbarContainerView;

// Convenience getter for the proxy object.
@property(nonatomic, readonly) CRWWebViewProxyImpl* contentViewProxy;

// Returns |self.bounds| after being inset at the top by the header height
// returned by the delegate.  This is only used to lay out native controllers,
// as the header height is already accounted for in the scroll view content
// insets for other CRWContentViews.
@property(nonatomic, readonly) CGRect visibleFrame;

@end

@implementation CRWWebControllerContainerView

- (instancetype)initWithDelegate:
        (id<CRWWebControllerContainerViewDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(delegate);
    _delegate.reset(delegate);
    self.backgroundColor = [UIColor whiteColor];
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
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

- (void)dealloc {
  self.contentViewProxy.contentView = nil;
  [super dealloc];
}

#pragma mark Accessors

- (CRWWebViewContentView*)webViewContentView {
  return _webViewContentView.get();
}

- (void)setWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  if (![_webViewContentView isEqual:webViewContentView]) {
    [_webViewContentView removeFromSuperview];
    _webViewContentView.reset([webViewContentView retain]);
    [_webViewContentView setFrame:self.bounds];
    [self addSubview:_webViewContentView];
  }
}

- (id<CRWNativeContent>)nativeController {
  return _nativeController.get();
}

- (void)setNativeController:(id<CRWNativeContent>)nativeController {
  if (![_nativeController isEqual:nativeController]) {
    base::WeakNSProtocol<id> oldController(_nativeController);
    [[oldController view] removeFromSuperview];
    _nativeController.reset([nativeController retain]);
    // TODO(crbug.com/503297): Re-enable this DCHECK once native controller
    // leaks are fixed.
    //    DCHECK(!oldController);
  }
}

- (CRWContentView*)transientContentView {
  return _transientContentView.get();
}

- (void)setTransientContentView:(CRWContentView*)transientContentView {
  if (![_transientContentView isEqual:transientContentView]) {
    [_transientContentView removeFromSuperview];
    _transientContentView.reset([transientContentView retain]);
  }
}

- (void)setToolbarContainerView:(CRWToolbarContainerView*)toolbarContainerView {
  if (![_toolbarContainerView isEqual:toolbarContainerView]) {
    [_toolbarContainerView removeFromSuperview];
    _toolbarContainerView.reset([toolbarContainerView retain]);
  }
}

- (UIView*)toolbarContainerView {
  return _toolbarContainerView.get();
}

- (CRWWebViewProxyImpl*)contentViewProxy {
  return [_delegate contentViewProxyForContainerView:self];
}

- (CGRect)visibleFrame {
  CGFloat headerHeight = [_delegate headerHeightForContainerView:self];
  return UIEdgeInsetsInsetRect(self.bounds,
                               UIEdgeInsetsMake(headerHeight, 0, 0, 0));
}

#pragma mark Layout

- (void)layoutSubviews {
  [super layoutSubviews];

  self.webViewContentView.frame = self.bounds;

  // TODO(crbug.com/570114): Move adding of the following subviews to another
  // place.

  // nativeController layout.
  if (self.nativeController) {
    UIView* nativeView = [self.nativeController view];
    if (!nativeView.superview) {
      [self addSubview:nativeView];
      [nativeView setNeedsUpdateConstraints];
    }
    nativeView.frame = self.visibleFrame;
  }

  // transientContentView layout.
  if (self.transientContentView) {
    if (!self.transientContentView.superview)
      [self addSubview:self.transientContentView];
    self.transientContentView.frame = self.bounds;
  }

  // Bottom align the toolbars with the bottom of the container.
  if (self.toolbarContainerView) {
    if (!self.toolbarContainerView.superview)
      [self addSubview:self.toolbarContainerView];
    else
      [self bringSubviewToFront:self.toolbarContainerView];
    CGSize toolbarContainerSize =
        [self.toolbarContainerView sizeThatFits:self.bounds.size];
    self.toolbarContainerView.frame =
        CGRectMake(CGRectGetMinX(self.bounds),
                   CGRectGetMaxY(self.bounds) - toolbarContainerSize.height,
                   toolbarContainerSize.width, toolbarContainerSize.height);
  }
}

- (BOOL)isViewAlive {
  return self.webViewContentView || self.transientContentView ||
         [self.nativeController isViewAlive];
}

#pragma mark Content Setters

- (void)resetContent {
  self.webViewContentView = nil;
  self.nativeController = nil;
  self.transientContentView = nil;
  [self removeAllToolbars];
  self.contentViewProxy.contentView = nil;
}

- (void)displayWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  DCHECK(webViewContentView);
  self.webViewContentView = webViewContentView;
  self.nativeController = nil;
  self.transientContentView = nil;
  self.contentViewProxy.contentView = self.webViewContentView;
  [self setNeedsLayout];
}

- (void)displayNativeContent:(id<CRWNativeContent>)nativeController {
  DCHECK(nativeController);
  self.webViewContentView = nil;
  self.nativeController = nativeController;
  self.transientContentView = nil;
  self.contentViewProxy.contentView = nil;
  [self setNeedsLayout];
}

- (void)displayTransientContent:(CRWContentView*)transientContentView {
  DCHECK(transientContentView);
  self.transientContentView = transientContentView;
  self.contentViewProxy.contentView = self.transientContentView;
  [self setNeedsLayout];
}

- (void)clearTransientContentView {
  self.transientContentView = nil;
  self.contentViewProxy.contentView = self.webViewContentView;
}

#pragma mark Toolbars

- (void)addToolbar:(UIView*)toolbar {
  // Create toolbar container if necessary.
  if (!self.toolbarContainerView) {
    self.toolbarContainerView = [
        [[CRWToolbarContainerView alloc] initWithFrame:CGRectZero] autorelease];
  }
  // Add the toolbar to the container.
  [self.toolbarContainerView addToolbar:toolbar];
  [self setNeedsLayout];
}

- (void)addToolbars:(NSArray*)toolbars {
  DCHECK(toolbars);
  for (UIView* toolbar in toolbars)
    [self addToolbar:toolbar];
}

- (void)removeToolbar:(UIView*)toolbar {
  // Remove the toolbar from the container view.
  [self.toolbarContainerView removeToolbar:toolbar];
  // Reset the container if there are no more toolbars.
  if ([self.toolbarContainerView.toolbars count])
    [self setNeedsLayout];
  else
    self.toolbarContainerView = nil;
}

- (void)removeAllToolbars {
  // Resetting the property will remove the toolbars from the hierarchy.
  self.toolbarContainerView = nil;
}

@end
