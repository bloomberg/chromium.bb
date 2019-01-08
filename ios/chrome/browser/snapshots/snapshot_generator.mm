// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_generator.h"

#include <algorithm>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_generator_delegate.h"
#import "ios/chrome/browser/snapshots/snapshot_overlay.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SnapshotGenerator ()<CRWWebStateObserver>

// Property providing access to the snapshot's cache. May be nil.
@property(nonatomic, readonly) SnapshotCache* snapshotCache;

// The unique ID for the web state.
@property(nonatomic, copy) NSString* sessionID;

// The associated web state.
@property(nonatomic, assign) web::WebState* webState;

@end

@implementation SnapshotGenerator {
  std::unique_ptr<web::WebStateObserver> _webStateObserver;
}

- (instancetype)initWithWebState:(web::WebState*)webState
               snapshotSessionId:(NSString*)snapshotSessionId {
  if ((self = [super init])) {
    DCHECK(webState);
    DCHECK(snapshotSessionId);
    _webState = webState;
    _sessionID = snapshotSessionId;

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

- (void)retrieveSnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);
  if (self.snapshotCache) {
    [self.snapshotCache retrieveImageForSessionID:self.sessionID
                                         callback:callback];
  } else {
    callback(nil);
  }
}

- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback {
  DCHECK(callback);

  __weak SnapshotGenerator* weakSelf = self;
  void (^wrappedCallback)(UIImage*) = ^(UIImage* image) {
    if (!image) {
      image = [weakSelf updateSnapshot];
      if (image)
        image = GreyImage(image);
    }
    callback(image);
  };

  SnapshotCache* snapshotCache = self.snapshotCache;
  if (snapshotCache) {
    [snapshotCache retrieveGreyImageForSessionID:self.sessionID
                                        callback:wrappedCallback];
  } else {
    wrappedCallback(nil);
  }
}

- (UIImage*)updateSnapshot {
  UIImage* snapshot = [self generateSnapshotWithOverlays:YES];
  [self updateSnapshotCacheWithImage:snapshot];
  return snapshot;
}

- (void)updateWebViewSnapshotWithCompletion:(void (^)(UIImage*))completion {
  DCHECK(self.webState);
  if (![self canTakeSnapshot]) {
    if (completion) {
      base::PostTaskWithTraits(FROM_HERE, {web::WebThread::UI},
                               base::BindOnce(^{
                                 completion(nil);
                               }));
    }
    return;
  }
  UIView* snapshotView = [self.delegate snapshotGenerator:self
                                      baseViewForWebState:self.webState];
  CGRect snapshotFrame =
      [self.webState->GetView() convertRect:[self snapshotFrame]
                                   fromView:snapshotView];
  NSArray<SnapshotOverlay*>* overlays =
      [self.delegate snapshotGenerator:self
           snapshotOverlaysForWebState:self.webState];

  [self.delegate snapshotGenerator:self
      willUpdateSnapshotForWebState:self.webState];
  __weak SnapshotGenerator* weakSelf = self;
  self.webState->TakeSnapshot(
      snapshotFrame, base::BindOnce(^(const gfx::Image& image) {
        UIImage* snapshot = [weakSelf snapshotWithOverlays:overlays
                                                     image:image
                                                     frame:snapshotFrame];
        [weakSelf updateSnapshotCacheWithImage:snapshot];
        if (completion)
          completion(snapshot);
      }));
}

- (UIImage*)generateSnapshotWithOverlays:(BOOL)shouldAddOverlay {
  if (![self canTakeSnapshot])
    return nil;
  CGRect frame = [self snapshotFrame];
  NSArray<SnapshotOverlay*>* overlays =
      shouldAddOverlay ? [self.delegate snapshotGenerator:self
                              snapshotOverlaysForWebState:self.webState]
                       : nil;

  [self.delegate snapshotGenerator:self
      willUpdateSnapshotForWebState:self.webState];
  UIView* view = [self.delegate snapshotGenerator:self
                              baseViewForWebState:self.webState];
  UIImage* snapshot = [self generateSnapshotForView:view
                                           withRect:frame
                                           overlays:overlays];
  return snapshot;
}

- (void)removeSnapshot {
  [self.snapshotCache removeImageWithSessionID:self.sessionID];
}

#pragma mark - Private methods

// Returns NO if WebState or the view is not ready for snapshot.
- (BOOL)canTakeSnapshot {
  // This allows for easier unit testing of classes that use SnapshotGenerator.
  if (!self.delegate)
    return NO;

  // Do not generate a snapshot if web usage is disabled (as the WebState's
  // view is blank in that case).
  if (!self.webState->IsWebUsageEnabled())
    return NO;

  return [self.delegate snapshotGenerator:self
               canTakeSnapshotForWebState:self.webState];
}

// Returns the frame of the snapshot.
- (CGRect)snapshotFrame {
  UIView* view = [self.delegate snapshotGenerator:self
                              baseViewForWebState:self.webState];
  UIEdgeInsets headerInsets = [self.delegate snapshotGenerator:self
                                 snapshotEdgeInsetsForWebState:self.webState];
  CGRect frame = UIEdgeInsetsInsetRect(view.bounds, headerInsets);
  DCHECK(!CGRectIsEmpty(frame));
  return frame;
}

// Takes a snapshot for the supplied view. Returns an autoreleased image cropped
// and scaled appropriately. The image can also contain overlays (if |overlays|
// is not nil and not empty).
- (UIImage*)generateSnapshotForView:(UIView*)view
                           withRect:(CGRect)rect
                           overlays:(NSArray<SnapshotOverlay*>*)overlays {
  DCHECK(view);
  DCHECK(!CGRectIsEmpty(rect));
  const CGFloat kScale =
      std::max<CGFloat>(1.0, [self.snapshotCache snapshotScaleForDevice]);
  UIGraphicsBeginImageContextWithOptions(rect.size, YES, kScale);
  CGContext* context = UIGraphicsGetCurrentContext();
  BOOL snapshotSuccess = YES;
  CGContextSaveGState(context);
  CGContextTranslateCTM(context, -rect.origin.x, -rect.origin.y);
  if (base::FeatureList::IsEnabled(kSnapshotDrawView)) {
    snapshotSuccess =
        [view drawViewHierarchyInRect:view.bounds afterScreenUpdates:NO];
  } else {
    [[view layer] renderInContext:context];
  }
  [self drawOverlays:overlays context:context];
  UIImage* image = nil;
  if (snapshotSuccess)
    image = UIGraphicsGetImageFromCurrentImageContext();
  CGContextRestoreGState(context);
  UIGraphicsEndImageContext();
  return image;
}

// Returns an image of the |image| overlaid with |overlays| with the given
// |frame|.
- (UIImage*)snapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                           image:(const gfx::Image&)image
                           frame:(CGRect)frame {
  DCHECK(!CGRectIsEmpty(frame));
  if (image.IsEmpty())
    return nil;
  if (overlays.count == 0)
    return image.ToUIImage();
  const CGFloat kScale =
      std::max<CGFloat>(1.0, [self.snapshotCache snapshotScaleForDevice]);
  UIGraphicsBeginImageContextWithOptions(frame.size, YES, kScale);
  CGContext* context = UIGraphicsGetCurrentContext();
  CGContextSaveGState(context);
  [image.ToUIImage() drawAtPoint:CGPointZero];
  [self drawOverlays:overlays context:context];
  UIImage* snapshotWithOverlays = UIGraphicsGetImageFromCurrentImageContext();
  CGContextRestoreGState(context);
  UIGraphicsEndImageContext();
  return snapshotWithOverlays;
}

// Updates the snapshot cache with |snapshot|.
- (void)updateSnapshotCacheWithImage:(UIImage*)snapshot {
  if (snapshot) {
    [self.snapshotCache setImage:snapshot withSessionID:self.sessionID];
  } else {
    // Remove any stale snapshot since the snapshot failed.
    [self.snapshotCache removeImageWithSessionID:self.sessionID];
  }
}

// Draws |overlays| onto |context|.
- (void)drawOverlays:(NSArray<SnapshotOverlay*>*)overlays
             context:(CGContext*)context {
  for (SnapshotOverlay* overlay in overlays) {
    // Render the overlay view at the desired offset. It is achieved
    // by shifting origin of context because view frame is ignored when
    // drawing to context.
    CGContextSaveGState(context);
    CGContextTranslateCTM(context, 0, overlay.yOffset);
    // |drawViewHierarchyInRect:| has undefined behavior when the view is not
    // in the visible view hierarchy. In practice, when this method is called
    // on a view that is part of view controller containment, an
    // UIViewControllerHierarchyInconsistency exception will be thrown.
    if (base::FeatureList::IsEnabled(kSnapshotDrawView) &&
        overlay.view.window) {
      [overlay.view drawViewHierarchyInRect:overlay.view.bounds
                         afterScreenUpdates:YES];
    } else {
      [[overlay.view layer] renderInContext:context];
    }
    CGContextRestoreGState(context);
  }
}

#pragma mark - Properties

- (SnapshotCache*)snapshotCache {
  return SnapshotCacheFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(
          self.webState->GetBrowserState()));
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _webState->RemoveObserver(_webStateObserver.get());
  _webStateObserver.reset();
  _webState = nullptr;
}

@end
