// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_generator.h"

// TODO(crbug.com/636188): required to implement ViewHierarchyContainsWKWebView
// for -drawViewHierarchyInRect:afterScreenUpdates:, remove once the workaround
// is no longer needed.
#import <WebKit/WebKit.h>

#include "base/logging.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/snapshot_overlay.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns YES if |view| or any view it contains is a WKWebView.
BOOL ViewHierarchyContainsWKWebView(UIView* view) {
  if ([view isKindOfClass:[WKWebView class]])
    return YES;
  for (UIView* subview in view.subviews) {
    if (ViewHierarchyContainsWKWebView(subview))
      return YES;
  }
  return NO;
}
}  // namespace

// Class that contains information used when caching snapshots of a web page.
@interface CoalescingSnapshotContext : NSObject

// Returns the cached snapshot if there is one matching the given parameters.
// Returns nil otherwise.
- (UIImage*)cachedSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly;

// Caches |snapshot| for the given |overlays| and |visibleFrameOnly|.
- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray<SnapshotOverlay*>*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly;

@end

@implementation CoalescingSnapshotContext {
  UIImage* _cachedSnapshot;
}

// Returns whether a snapshot should be cached in a page loaded context.
// Note: Returns YES if |overlays| is nil or empty and if |visibleFrameOnly| is
// YES as this is the only case when the snapshot taken for the page is reused.
- (BOOL)shouldCacheSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                       visibleFrameOnly:(BOOL)visibleFrameOnly {
  return visibleFrameOnly && ![overlays count];
}

- (UIImage*)cachedSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly {
  if ([self shouldCacheSnapshotWithOverlays:overlays
                           visibleFrameOnly:visibleFrameOnly]) {
    return _cachedSnapshot;
  }
  return nil;
}

- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray<SnapshotOverlay*>*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly {
  if ([self shouldCacheSnapshotWithOverlays:overlays
                           visibleFrameOnly:visibleFrameOnly]) {
    DCHECK(!_cachedSnapshot);
    _cachedSnapshot = snapshot;
  }
}

@end

@interface SnapshotGenerator ()<CRWWebStateObserver>

// Takes a snapshot for the supplied view (which should correspond to the given
// type of web view). Returns an autoreleased image cropped and scaled
// appropriately. The image can also contain overlays (if |overlays| is not
// nil and not empty).
- (UIImage*)generateSnapshotForView:(UIView*)view
                           withRect:(CGRect)rect
                           overlays:(NSArray<SnapshotOverlay*>*)overlays;

@end

@implementation SnapshotGenerator {
  CoalescingSnapshotContext* _coalescingSnapshotContext;
  std::unique_ptr<web::WebStateObserver> _webStateObserver;
  __weak id<SnapshotGeneratorDelegate> _delegate;
  web::WebState* _webState;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                        delegate:(id<SnapshotGeneratorDelegate>)delegate {
  if ((self = [super init])) {
    DCHECK(webState);
    _webState = webState;
    _delegate = delegate;

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserver.get());
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());
    _webStateObserver.reset();
    _webState = nullptr;
  }
}

- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled {
  if (snapshotCoalescingEnabled) {
    DCHECK(!_coalescingSnapshotContext);
    _coalescingSnapshotContext = [[CoalescingSnapshotContext alloc] init];
  } else {
    DCHECK(_coalescingSnapshotContext);
    _coalescingSnapshotContext = nil;
  }
}

- (void)retrieveSnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);
  void (^wrappedCallback)(UIImage*) = ^(UIImage* image) {
    if (!image) {
      image = [self updateSnapshotWithOverlays:YES visibleFrameOnly:YES];
    }
    callback(image);
  };

  NSString* sessionID = TabIdTabHelper::FromWebState(_webState)->tab_id();
  [[self snapshotCache] retrieveImageForSessionID:sessionID
                                         callback:wrappedCallback];
}

- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);
  void (^wrappedCallback)(UIImage*) = ^(UIImage* image) {
    if (!image) {
      image = [self updateSnapshotWithOverlays:YES visibleFrameOnly:YES];
      image = GreyImage(image);
    }
    callback(image);
  };

  NSString* sessionID = TabIdTabHelper::FromWebState(_webState)->tab_id();
  [[self snapshotCache] retrieveGreyImageForSessionID:sessionID
                                             callback:wrappedCallback];
}

- (UIImage*)updateSnapshotWithOverlays:(BOOL)shouldAddOverlay
                      visibleFrameOnly:(BOOL)visibleFrameOnly {
  UIImage* snapshot = [self generateSnapshotWithOverlays:shouldAddOverlay
                                        visibleFrameOnly:visibleFrameOnly];

  // Return default snapshot without caching it if the generation failed.
  if (!snapshot)
    return [_delegate defaultSnapshotImage];

  UIImage* snapshotToCache = snapshot;
  if (!visibleFrameOnly) {
    UIEdgeInsets insets = [_delegate snapshotEdgeInsets];
    if (!UIEdgeInsetsEqualToEdgeInsets(insets, UIEdgeInsetsZero)) {
      CGRect cropRect =
          UIEdgeInsetsInsetRect(CGRect{CGPointZero, [snapshot size]}, insets);
      snapshotToCache = CropImage(snapshot, cropRect);
    }
  }

  NSString* sessionID = TabIdTabHelper::FromWebState(_webState)->tab_id();
  [[self snapshotCache] setImage:snapshotToCache withSessionID:sessionID];
  return snapshot;
}

- (UIImage*)generateSnapshotWithOverlays:(BOOL)shouldAddOverlay
                        visibleFrameOnly:(BOOL)visibleFrameOnly {
  if (!_webState->CanTakeSnapshot())
    return nil;

  UIView* view = _webState->GetView();
  CGRect bounds = [view bounds];
  if (visibleFrameOnly) {
    UIEdgeInsets insets = [_delegate snapshotEdgeInsets];
    bounds = UIEdgeInsetsInsetRect(bounds, insets);
  }

  if (CGRectIsEmpty(bounds))
    return nil;

  NSArray<SnapshotOverlay*>* overlays =
      shouldAddOverlay ? [_delegate snapshotOverlays] : nil;
  UIImage* snapshot =
      [_coalescingSnapshotContext cachedSnapshotWithOverlays:overlays
                                            visibleFrameOnly:visibleFrameOnly];

  if (snapshot)
    return snapshot;

  [_delegate willUpdateSnapshotForWebState:_webState];
  snapshot =
      [self generateSnapshotForView:view withRect:bounds overlays:overlays];
  [_coalescingSnapshotContext setCachedSnapshot:snapshot
                                   withOverlays:overlays
                               visibleFrameOnly:visibleFrameOnly];
  [_delegate didUpdateSnapshotForWebState:_webState withImage:snapshot];
  return snapshot;
}

#pragma mark - Private methods

- (UIImage*)generateSnapshotForView:(UIView*)view
                           withRect:(CGRect)rect
                           overlays:(NSArray<SnapshotOverlay*>*)overlays {
  DCHECK(view);
  CGSize size = rect.size;
  DCHECK(std::isnormal(size.width) && (size.width > 0))
      << ": size.width=" << size.width;
  DCHECK(std::isnormal(size.height) && (size.height > 0))
      << ": size.height=" << size.height;
  const CGFloat kScale = [[self snapshotCache] snapshotScaleForDevice];
  UIGraphicsBeginImageContextWithOptions(size, YES, kScale);
  CGContext* context = UIGraphicsGetCurrentContext();
  DCHECK(context);

  // TODO(crbug.com/636188): -drawViewHierarchyInRect:afterScreenUpdates: is
  // buggy on iOS 8/9/10 (and state is unknown for iOS 11) causing GPU glitches,
  // screen redraws during animations, broken pinch to dismiss on tablet, etc.
  // For the moment, only use it for WKWebView with depends on it. Remove this
  // check and always use -drawViewHierarchyInRect:afterScreenUpdates: once it
  // is working correctly in all version of iOS supported.
  BOOL useDrawViewHierarchy = ViewHierarchyContainsWKWebView(view);

  BOOL snapshotSuccess = YES;
  CGContextSaveGState(context);
  CGContextTranslateCTM(context, -rect.origin.x, -rect.origin.y);
  if (useDrawViewHierarchy) {
    snapshotSuccess =
        [view drawViewHierarchyInRect:view.bounds afterScreenUpdates:NO];
  } else {
    [[view layer] renderInContext:context];
  }
  if ([overlays count]) {
    for (SnapshotOverlay* overlay in overlays) {
      // Render the overlay view at the desired offset. It is achieved
      // by shifting origin of context because view frame is ignored when
      // drawing to context.
      CGContextSaveGState(context);
      CGContextTranslateCTM(context, 0, overlay.yOffset);
      if (useDrawViewHierarchy) {
        [overlay.view drawViewHierarchyInRect:overlay.view.bounds
                           afterScreenUpdates:YES];
      } else {
        [[overlay.view layer] renderInContext:context];
      }
      CGContextRestoreGState(context);
    }
  }
  UIImage* image = nil;
  if (snapshotSuccess)
    image = UIGraphicsGetImageFromCurrentImageContext();
  CGContextRestoreGState(context);
  UIGraphicsEndImageContext();
  return image;
}

- (SnapshotCache*)snapshotCache {
  DCHECK(_webState);
  return SnapshotCacheFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(_webState->GetBrowserState()));
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webState = nullptr;
}

@end
