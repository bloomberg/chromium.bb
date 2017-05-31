// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_manager.h"

#import <QuartzCore/QuartzCore.h>
#import <WebKit/WebKit.h>

#include "base/logging.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_overlay.h"

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

@implementation SnapshotManager

- (UIImage*)generateSnapshotForView:(UIView*)view
                           withRect:(CGRect)rect
                           overlays:(NSArray*)overlays {
  DCHECK(view);
  CGSize size = rect.size;
  DCHECK(std::isnormal(size.width) && (size.width > 0))
      << ": size.width=" << size.width;
  DCHECK(std::isnormal(size.height) && (size.height > 0))
      << ": size.height=" << size.height;
  const CGFloat kScale =
      [[SnapshotCache sharedInstance] snapshotScaleForDevice];
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

- (void)setImage:(UIImage*)image withSessionID:(NSString*)sessionID {
  [[SnapshotCache sharedInstance] setImage:image withSessionID:sessionID];
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
