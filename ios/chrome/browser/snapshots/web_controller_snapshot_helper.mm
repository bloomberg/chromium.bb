// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/web_controller_snapshot_helper.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_overlay.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_headers_delegate.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

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
// YES as this is the only case when the snapshot taken by the CRWWebController
// is reused.
- (BOOL)shouldCacheSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                       visibleFrameOnly:(BOOL)visibleFrameOnly {
  return ![overlays count] && visibleFrameOnly;
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

@interface WebControllerSnapshotHelper ()

// Takes a snapshot image for the WebController's current page. Returns an
// autoreleased image cropped and scaled appropriately. Returns a default image
// if a snapshot cannot be generated.
- (UIImage*)
generateSnapshotOrDefaultForWebController:(CRWWebController*)webController
                             withOverlays:(NSArray<SnapshotOverlay*>*)overlays
                         visibleFrameOnly:(BOOL)visibleFrameOnly;

// Returns the cached snapshot if there is one matching the given parameters.
// Returns nil otherwise or if there is no |_coalescingSnapshotContext|.
- (UIImage*)cachedSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly;

// Caches |snapshot| for the given |overlays| and |visibleFrameOnly|. Does
// nothing if there is no |_coalescingSnapshotContext|.
- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray<SnapshotOverlay*>*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly;

// Takes a snapshot for the supplied view (which should correspond to the given
// type of web view). Returns an autoreleased image cropped and scaled
// appropriately.
// The image is not yet cached.
// The image can also contain overlays (if |overlays| is not nil and not empty).
- (UIImage*)generateSnapshotForView:(UIView*)view
                           withRect:(CGRect)rect
                           overlays:(NSArray<SnapshotOverlay*>*)overlays;

@end

@implementation WebControllerSnapshotHelper {
  CoalescingSnapshotContext* _coalescingSnapshotContext;
  __weak CRWWebController* _webController;
  // Owns this WebControllerSnapshotHelper.
  __weak Tab* _tab;
}

- (instancetype)initWithTab:(Tab*)tab {
  if ((self = [super init])) {
    DCHECK(tab);
    DCHECK(tab.tabId);
    DCHECK([tab webController]);
    _webController = [tab webController];
    _tab = tab;
  }
  return self;
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

- (void)retrieveSnapshotForWebController:(CRWWebController*)webController
                               sessionID:(NSString*)sessionID
                            withOverlays:(NSArray<SnapshotOverlay*>*)overlays
                                callback:(void (^)(UIImage* image))callback {
  [[self snapshotCache]
      retrieveImageForSessionID:sessionID
                       callback:^(UIImage* image) {
                         if (image) {
                           callback(image);
                         } else {
                           callback([self
                               updateSnapshotForWebController:webController
                                                    sessionID:sessionID
                                                 withOverlays:overlays
                                             visibleFrameOnly:YES]);
                         }
                       }];
}

- (void)
retrieveGreySnapshotForWebController:(CRWWebController*)webController
                           sessionID:(NSString*)sessionID
                        withOverlays:(NSArray<SnapshotOverlay*>*)overlays
                            callback:(void (^)(UIImage* image))callback {
  [[self snapshotCache]
      retrieveGreyImageForSessionID:sessionID
                           callback:^(UIImage* image) {
                             if (image) {
                               callback(image);
                             } else {
                               callback(GreyImage([self
                                   updateSnapshotForWebController:webController
                                                        sessionID:sessionID
                                                     withOverlays:overlays
                                                 visibleFrameOnly:YES]));
                             }
                           }];
}

- (UIImage*)updateSnapshotForWebController:(CRWWebController*)webController
                                 sessionID:(NSString*)sessionID
                              withOverlays:(NSArray<SnapshotOverlay*>*)overlays
                          visibleFrameOnly:(BOOL)visibleFrameOnly {
  // TODO(crbug.com/661641): This is a temporary solution to make sure the
  // webController retained by us is the same being passed in in this method.
  // Remove this when all uses of the "CRWWebController" are dropped from the
  // methods names since it is already retained by us and not necessary to be
  // passed in.
  DCHECK_EQ([_tab webController], webController);
  UIImage* snapshot =
      [self generateSnapshotOrDefaultForWebController:webController
                                         withOverlays:overlays
                                     visibleFrameOnly:visibleFrameOnly];
  // If the snapshot is the default one, return it without caching it.
  if (snapshot == [[self class] defaultSnapshotImage])
    return snapshot;

  UIImage* snapshotToCache = nil;
  // TODO(crbug.com/370994): Remove all code that references a Tab's delegates
  // from this file.
  if (visibleFrameOnly || ![_tab legacyFullscreenControllerDelegate]) {
    snapshotToCache = snapshot;
  } else {
    // Crops the bottom of the fullscreen snapshot.
    CGRect cropRect =
        CGRectMake(0, [[_tab tabHeadersDelegate] headerHeightForTab:_tab],
                   [snapshot size].width, [snapshot size].height);
    snapshotToCache = CropImage(snapshot, cropRect);
  }
  [[self snapshotCache] setImage:snapshotToCache withSessionID:sessionID];
  return snapshot;
}

- (UIImage*)generateSnapshotForWebController:(CRWWebController*)webController
                                withOverlays:
                                    (NSArray<SnapshotOverlay*>*)overlays
                            visibleFrameOnly:(BOOL)visibleFrameOnly {
  if (![webController canUseViewForGeneratingOverlayPlaceholderView])
    return nil;
  CGRect visibleFrame = (visibleFrameOnly ? [_tab snapshotContentArea]
                                          : [webController.view bounds]);
  if (CGRectIsEmpty(visibleFrame))
    return nil;
  UIImage* snapshot = [self cachedSnapshotWithOverlays:overlays
                                      visibleFrameOnly:visibleFrameOnly];
  if (!snapshot) {
    [_tab willUpdateSnapshot];
    snapshot = [self generateSnapshotForView:webController.view
                                    withRect:visibleFrame
                                    overlays:overlays];
    [self setCachedSnapshot:snapshot
               withOverlays:overlays
           visibleFrameOnly:visibleFrameOnly];
  }
  return snapshot;
}

#pragma mark - Private methods

- (UIImage*)
generateSnapshotOrDefaultForWebController:(CRWWebController*)webController
                             withOverlays:(NSArray<SnapshotOverlay*>*)overlays
                         visibleFrameOnly:(BOOL)visibleFrameOnly {
  UIImage* result = [self generateSnapshotForWebController:webController
                                              withOverlays:overlays
                                          visibleFrameOnly:visibleFrameOnly];
  return result ? result : [[self class] defaultSnapshotImage];
}

- (UIImage*)cachedSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly {
  return
      [_coalescingSnapshotContext cachedSnapshotWithOverlays:overlays
                                            visibleFrameOnly:visibleFrameOnly];
}

- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray<SnapshotOverlay*>*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly {
  return [_coalescingSnapshotContext setCachedSnapshot:snapshot
                                          withOverlays:overlays
                                      visibleFrameOnly:visibleFrameOnly];
}

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
  if (!context) {
    NOTREACHED();
    return nil;
  }

  // -drawViewHierarchyInRect:afterScreenUpdates:YES is buggy as of iOS 8.3.
  // Using it afterScreenUpdates:YES creates unexpected GPU glitches, screen
  // redraws during animations, broken pinch to dismiss on tablet, etc.  For now
  // only using this with WKWebView, which depends on -drawViewHierarchyInRect.
  // TODO(justincohen): Remove this (and always use drawViewHierarchyInRect)
  // once the iOS 8 bugs have been fixed.
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

// TODO(crbug.com/661642): This code is shared with CRWWebController. Remove
// this and add it as part of WebDelegate delegate API such that a default
// image is returned immediately.
+ (UIImage*)defaultSnapshotImage {
  static UIImage* defaultImage = nil;

  if (!defaultImage) {
    CGRect frame = CGRectMake(0, 0, 2, 2);
    UIGraphicsBeginImageContext(frame.size);
    [[UIColor whiteColor] setFill];
    CGContextFillRect(UIGraphicsGetCurrentContext(), frame);

    UIImage* result = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();

    defaultImage = [result stretchableImageWithLeftCapWidth:1 topCapHeight:1];
  }
  return defaultImage;
}

- (SnapshotCache*)snapshotCache {
  return SnapshotCacheFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(
          _webController.webState->GetBrowserState()));
}

@end
