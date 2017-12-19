// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_

#import <UIKit/UIKit.h>

@class SnapshotOverlay;
@protocol SnapshotGeneratorDelegate;

namespace web {
class WebState;
}

// A class that takes care of creating, storing and returning snapshots of a
// tab's web page.
@interface SnapshotGenerator : NSObject

// Designated initializer.
- (instancetype)initWithWebState:(web::WebState*)webState
                        delegate:(id<SnapshotGeneratorDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// If |snapshotCoalescingEnabled| is YES snapshots of the web page are
// coalesced until this method is called with |snapshotCoalescingEnabled| set to
// NO. When snapshot coalescing is enabled, mutiple calls to generate a snapshot
// with the same parameters may be coalesced.
- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled;

// Gets a color snapshot for the current page, calling |callback| once it has
// been retrieved or regenerated.
- (void)retrieveSnapshot:(void (^)(UIImage*))callback;

// Gets a grey snapshot for the current page, calling |callback| once it has
// been retrieved or regenerated.
- (void)retrieveGreySnapshot:(void (^)(UIImage*))callback;

// Invalidates the cached snapshot for the current page, generates and caches
// a new snapshot. Returns the snapshot with or without the overlayed views
// (e.g. infobar, voice search button, etc.), and either of the visible frame
// or of the full screen.
- (UIImage*)updateSnapshotWithOverlays:(BOOL)shouldAddOverlay
                      visibleFrameOnly:(BOOL)visibleFrameOnly;

// Generates a new snapshot for the current page including optional infobars.
// Returns the snapshot with or without the overlayed views (e.g. infobar,
// voice search button, etc.), and either of the visible frame or of the full
// screen.
- (UIImage*)generateSnapshotWithOverlays:(BOOL)shouldAddOverlay
                        visibleFrameOnly:(BOOL)visibleFrameOnly;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_
