// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_

#import <UIKit/UIKit.h>

@class SnapshotOverlay;
@class Tab;

// A class that takes care of creating, storing and returning snapshots of a
// tab's web page.
@interface SnapshotGenerator : NSObject

// Designated initializer. |tab| must not be nil.
// TODO(crbug.com/380819): Replace the need to use Tab directly here by using a
// delegate pattern.
- (instancetype)initWithTab:(Tab*)tab NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// If |snapshotCoalescingEnabled| is YES snapshots of the web page are
// coalesced until this method is called with |snapshotCoalescingEnabled| set to
// NO. When snapshot coalescing is enabled, mutiple calls to generate a snapshot
// with the same parameters may be coalesced.
- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled;

// Gets a color snapshot for the current page, calling |callback| once it has
// been retrieved or regenerated. |overlays| is the array of SnapshotOverlay
// objects (views currently overlayed), can be nil.
- (void)retrieveSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                            callback:(void (^)(UIImage*))callback;

// Gets a grey snapshot for the current page, calling |callback| once it has
// been retrieved or regenerated. |overlays| is the array of SnapshotOverlay
// objects (views currently overlayed), can be nil.
- (void)retrieveGreySnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                                callback:(void (^)(UIImage*))callback;

// Invalidates the cached snapshot for the current page, generates and caches
// a new snapshot. Returns the snapshot with or without the overlayed views
// (e.g. infobar, voice search button, etc.), and either of the visible frame
// or of the full screen. |overlays| is the array of SnapshotOverlay objects
// (views currently overlayed), can be nil.
- (UIImage*)updateSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                      visibleFrameOnly:(BOOL)visibleFrameOnly;

// Generates a new snapshot for the current page including optional infobars.
// Returns the snapshot with or without the overlayed views (e.g. infobar,
// voice search button, etc.), and either of the visible frame or of the full
// screen. |overlays| is the array of SnapshotOverlay objects (views currently
// overlayed), can be nil.
- (UIImage*)generateSnapshotWithOverlays:(NSArray<SnapshotOverlay*>*)overlays
                        visibleFrameOnly:(BOOL)visibleFrameOnly;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_GENERATOR_H_
