// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_manager.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_overlay.h"

@implementation SnapshotManager

- (UIImage*)generateSnapshotForView:(UIView*)view
                           withRect:(CGRect)rect
                           overlays:(NSArray*)overlays {
  DCHECK(view);
  CGSize size = rect.size;
  DCHECK(isnormal(size.width) && (size.width > 0))
      << ": size.width=" << size.width;
  DCHECK(isnormal(size.height) && (size.height > 0))
      << ": size.height=" << size.height;
  const CGFloat kScale = [SnapshotCache snapshotScaleForDevice];
  UIGraphicsBeginImageContextWithOptions(size, YES, kScale);
  CGContext* context = UIGraphicsGetCurrentContext();
  if (!context) {
    NOTREACHED();
    return nil;
  }

  CGContextSaveGState(context);
  CGContextTranslateCTM(context, -rect.origin.x, -rect.origin.y);
#if defined(ENABLE_WKWEBVIEW)
  // -drawViewHierarchyInRect:afterScreenUpdates:YES is buggy as of iOS 8.1.1.
  // Using it afterScreenUpdates:YES creates unexpected GPU glitches, screen
  // redraws during animations, broken pinch to dismiss on tablet, etc.  For now
  // only using this with ENABLE_WKWEBVIEW, which depends on
  // -drawViewHierarchyInRect.
  [view drawViewHierarchyInRect:view.bounds afterScreenUpdates:YES];
#else
  [[view layer] renderInContext:context];
#endif
  if ([overlays count]) {
    for (SnapshotOverlay* overlay in overlays) {
      // Render the overlay view at the desired offset. It is achieved
      // by shifting origin of context because view frame is ignored when
      // drawing to context.
      CGContextSaveGState(context);
      CGContextTranslateCTM(context, 0, overlay.yOffset);
#if defined(ENABLE_WKWEBVIEW)
      CGRect overlayRect = overlay.view.bounds;
      // TODO(jyquinn): The 0 check is needed for a UIKit crash on iOS 7.  This
      // can be removed once iOS 7 is dropped. crbug.com/421213
      if (overlayRect.size.width > 0 && overlayRect.size.height > 0) {
        [overlay.view drawViewHierarchyInRect:overlay.view.bounds
                           afterScreenUpdates:YES];
      }
#else
      [[overlay.view layer] renderInContext:context];
#endif
      CGContextRestoreGState(context);
    }
  }
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  CGContextRestoreGState(context);
  UIGraphicsEndImageContext();
  return image;
}

- (void)retrieveImageForSessionID:(NSString*)sessionID
                         callback:(void (^)(UIImage*))callback {
  [[SnapshotCache sharedInstance] retrieveImageForSessionID:sessionID
                                                   callback:callback];
}

- (void)retrieveGreyImageForSessionID:(NSString*)sessionID
                             callback:(void (^)(UIImage*))callback {
  [[SnapshotCache sharedInstance] retrieveGreyImageForSessionID:sessionID
                                                       callback:callback];
}

- (void)setImage:(UIImage*)img withSessionID:(NSString*)sessionID {
  [[SnapshotCache sharedInstance] setImage:img withSessionID:sessionID];
}

- (void)removeImageWithSessionID:(NSString*)sessionID {
  [[SnapshotCache sharedInstance] removeImageWithSessionID:sessionID];
}

- (void)greyImageForSessionID:(NSString*)sessionID
                     callback:(void (^)(UIImage*))callback {
  [[SnapshotCache sharedInstance] greyImageForSessionID:sessionID
                                               callback:callback];
}

@end
