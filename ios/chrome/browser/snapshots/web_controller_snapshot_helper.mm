// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/web_controller_snapshot_helper.h"

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/snapshots/snapshot_manager.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_headers_delegate.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

@interface WebControllerSnapshotHelper ()

// Takes a snapshot image for the WebController's current page. Returns an
// autoreleased image cropped and scaled appropriately. Returns a default image
// if a snapshot cannot be generated.
- (UIImage*)generateSnapshotOrDefaultForWebController:
                (CRWWebController*)webController
                                         withOverlays:(NSArray*)overlays
                                     visibleFrameOnly:(BOOL)visibleFrameOnly;

// Returns the cached snapshot if there is one matching the given parameters.
// Returns nil otherwise or if there is no |_coalescingSnapshotContext|.
- (UIImage*)cachedSnapshotWithOverlays:(NSArray*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly;

// Caches |snapshot| for the given |overlays| and |visibleFrameOnly|. Does
// nothing if there is no |_coalescingSnapshotContext|.
- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly;
@end

// Class that contains information used when caching snapshots of a web page.
@interface CoalescingSnapshotContext : NSObject

// Returns the cached snapshot if there is one matching the given parameters.
// Returns nil otherwise.
- (UIImage*)cachedSnapshotWithOverlays:(NSArray*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly;

// Caches |snapshot| for the given |overlays| and |visibleFrameOnly|.
- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly;

@end

@implementation CoalescingSnapshotContext {
  base::scoped_nsobject<UIImage> _cachedSnapshot;
}

// Returns whether a snapshot should be cached in a page loaded context.
// Note: Returns YES if |overlays| is nil or empty and if |visibleFrameOnly| is
// YES as this is the only case when the snapshot taken by the CRWWebController
// is reused.
- (BOOL)shouldCacheSnapshotWithOverlays:(NSArray*)overlays
                       visibleFrameOnly:(BOOL)visibleFrameOnly {
  return ![overlays count] && visibleFrameOnly;
}

- (UIImage*)cachedSnapshotWithOverlays:(NSArray*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly {
  if ([self shouldCacheSnapshotWithOverlays:overlays
                           visibleFrameOnly:visibleFrameOnly]) {
    return _cachedSnapshot;
  }
  return nil;
}

- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly {
  if ([self shouldCacheSnapshotWithOverlays:overlays
                           visibleFrameOnly:visibleFrameOnly]) {
    DCHECK(!_cachedSnapshot);
    _cachedSnapshot.reset([snapshot retain]);
  }
}

@end

@implementation WebControllerSnapshotHelper {
  base::scoped_nsobject<CoalescingSnapshotContext> _coalescingSnapshotContext;
  base::scoped_nsobject<SnapshotManager> _snapshotManager;
  base::WeakNSObject<CRWWebController> _webController;
  // Owns this WebControllerSnapshotHelper.
  base::WeakNSObject<Tab> _tab;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithSnapshotManager:(SnapshotManager*)snapshotManager
                                    tab:(Tab*)tab {
  self = [super init];
  if (self) {
    DCHECK(snapshotManager);
    DCHECK(tab);
    DCHECK(tab.tabId);
    DCHECK([tab webController]);
    _snapshotManager.reset([snapshotManager retain]);
    _webController.reset([tab webController]);
    _tab.reset(tab);
  }
  return self;
}

- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled {
  if (snapshotCoalescingEnabled) {
    DCHECK(!_coalescingSnapshotContext);
    _coalescingSnapshotContext.reset([[CoalescingSnapshotContext alloc] init]);
  } else {
    DCHECK(_coalescingSnapshotContext);
    _coalescingSnapshotContext.reset();
  }
}

- (UIImage*)generateSnapshotOrDefaultForWebController:
                (CRWWebController*)webController
                                         withOverlays:(NSArray*)overlays
                                     visibleFrameOnly:(BOOL)visibleFrameOnly {
  UIImage* result = [self generateSnapshotForWebController:webController
                                              withOverlays:overlays
                                          visibleFrameOnly:visibleFrameOnly];
  return result ? result : [[self class] defaultSnapshotImage];
}

- (void)retrieveSnapshotForWebController:(CRWWebController*)webController
                               sessionID:(NSString*)sessionID
                            withOverlays:(NSArray*)overlays
                                callback:(void (^)(UIImage* image))callback {
  [_snapshotManager
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

- (void)retrieveGreySnapshotForWebController:(CRWWebController*)webController
                                   sessionID:(NSString*)sessionID
                                withOverlays:(NSArray*)overlays
                                    callback:
                                        (void (^)(UIImage* image))callback {
  [_snapshotManager
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
                              withOverlays:(NSArray*)overlays
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
  if (visibleFrameOnly || ![_tab fullScreenControllerDelegate]) {
    snapshotToCache = snapshot;
  } else {
    // Crops the bottom of the fullscreen snapshot.
    CGRect cropRect =
        CGRectMake(0, [[_tab tabHeadersDelegate] headerHeightForTab:_tab],
                   [snapshot size].width, [snapshot size].height);
    snapshotToCache = CropImage(snapshot, cropRect);
  }
  [_snapshotManager setImage:snapshotToCache withSessionID:sessionID];
  return snapshot;
}

- (UIImage*)generateSnapshotForWebController:(CRWWebController*)webController
                                withOverlays:(NSArray*)overlays
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
    snapshot = [_snapshotManager generateSnapshotForView:webController.view
                                                withRect:visibleFrame
                                                overlays:overlays];
    [self setCachedSnapshot:snapshot
               withOverlays:overlays
           visibleFrameOnly:visibleFrameOnly];
  }
  return snapshot;
}

// TODO(crbug.com/661642): This code is shared with SnapshotManager. Remove this
// and add it as part of WebDelegate delegate API such that a default image is
// returned immediately.
+ (UIImage*)defaultSnapshotImage {
  static UIImage* defaultImage = nil;

  if (!defaultImage) {
    CGRect frame = CGRectMake(0, 0, 2, 2);
    UIGraphicsBeginImageContext(frame.size);
    [[UIColor whiteColor] setFill];
    CGContextFillRect(UIGraphicsGetCurrentContext(), frame);

    UIImage* result = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();

    defaultImage =
        [[result stretchableImageWithLeftCapWidth:1 topCapHeight:1] retain];
  }
  return defaultImage;
}

- (void)setCachedSnapshot:(UIImage*)snapshot
             withOverlays:(NSArray*)overlays
         visibleFrameOnly:(BOOL)visibleFrameOnly {
  return [_coalescingSnapshotContext setCachedSnapshot:snapshot
                                          withOverlays:overlays
                                      visibleFrameOnly:visibleFrameOnly];
}

- (UIImage*)cachedSnapshotWithOverlays:(NSArray*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly {
  return
      [_coalescingSnapshotContext cachedSnapshotWithOverlays:overlays
                                            visibleFrameOnly:visibleFrameOnly];
}

@end
