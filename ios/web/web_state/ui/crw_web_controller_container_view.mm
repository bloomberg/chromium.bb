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
  // The proxy for the content added to the container.  It is owned by the web
  // controller.
  base::WeakNSObject<CRWWebViewProxyImpl> _webViewProxy;
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

@end

@implementation CRWWebControllerContainerView

- (instancetype)initWithContentViewProxy:(CRWWebViewProxyImpl*)proxy {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(proxy);
    _webViewProxy.reset(proxy);
    self.backgroundColor = [UIColor whiteColor];
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  }
  return self;
}

- (void)dealloc {
  [_webViewProxy setContentView:nil];
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
    [self addSubview:_webViewContentView];
  }
}

- (id<CRWNativeContent>)nativeController {
  return _nativeController.get();
}

- (void)setNativeController:(id<CRWNativeContent>)nativeController {
  if (![_nativeController isEqual:nativeController]) {
    // TODO(kkhorimoto): This line isn't strictly necessary since all native
    // controllers currently inherit from NativeContentController, which removes
    // its view upon deallocation.  Consider moving NativeContentController into
    // web/ so this behavior can be depended upon from within web/ without
    // making assumptions about chrome/ code.
    base::WeakNSProtocol<id> oldController(_nativeController);
    [[_nativeController view] removeFromSuperview];
    _nativeController.reset([nativeController retain]);
    [self addSubview:[_nativeController view]];
    [[_nativeController view] setNeedsUpdateConstraints];
    DCHECK(!oldController);
  }
}

- (CRWContentView*)transientContentView {
  return _transientContentView.get();
}

- (void)setTransientContentView:(CRWContentView*)transientContentView {
  if (![_transientContentView isEqual:transientContentView]) {
    [_transientContentView removeFromSuperview];
    _transientContentView.reset([transientContentView retain]);
    [self addSubview:_transientContentView];
  }
}

- (void)setToolbarContainerView:(CRWToolbarContainerView*)toolbarContainerView {
  if (![_toolbarContainerView isEqual:toolbarContainerView]) {
    [_toolbarContainerView removeFromSuperview];
    _toolbarContainerView.reset([toolbarContainerView retain]);
    [self addSubview:_toolbarContainerView];
  }
}

- (UIView*)toolbarContainerView {
  return _toolbarContainerView.get();
}

#pragma mark Layout

- (void)layoutSubviews {
  [super layoutSubviews];

  // Resize displayed content to the container's bounds.
  self.webViewContentView.frame = self.bounds;
  [self.nativeController view].frame = self.bounds;
  self.transientContentView.frame = self.bounds;

  // Bottom align the toolbars with the bottom of the container.
  if (self.toolbarContainerView) {
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
  [_webViewProxy setContentView:nil];
}

- (void)displayWebViewContentView:(CRWWebViewContentView*)webViewContentView {
  DCHECK(webViewContentView);
  self.webViewContentView = webViewContentView;
  self.nativeController = nil;
  self.transientContentView = nil;
  [_webViewProxy setContentView:self.webViewContentView];
  [self setNeedsLayout];
}

- (void)displayNativeContent:(id<CRWNativeContent>)nativeController {
  DCHECK(nativeController);
  self.webViewContentView = nil;
  self.nativeController = nativeController;
  self.transientContentView = nil;
  [_webViewProxy setContentView:nil];
  [self setNeedsLayout];
}

- (void)displayTransientContent:(CRWContentView*)transientContentView {
  DCHECK(transientContentView);
  self.transientContentView = transientContentView;
  [_webViewProxy setContentView:self.transientContentView];
  [self setNeedsLayout];
}

- (void)clearTransientContentView {
  self.transientContentView = nil;
  [_webViewProxy setContentView:self.webViewContentView];
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
