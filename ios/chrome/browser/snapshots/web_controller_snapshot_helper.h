// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_WEB_CONTROLLER_SNAPSHOT_HELPER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_WEB_CONTROLLER_SNAPSHOT_HELPER_H_

#import <UIKit/UIKit.h>

@class CRWWebController;
@class SnapshotManager;
@class Tab;

// A class that takes care of creating, storing and returning snapshots of a
// WebController's web page.
// TODO(crbug.com/661642): There is a lot of overlap/common functionality
// between this class and SnapshotManager, coalesce these 2 classes.
@interface WebControllerSnapshotHelper : NSObject

// Designated initializer. |snapshotManager|, |sessionID|, |webController|
// should not be nil.
// TODO(crbug.com/661641): Since we already retain a CRWWebController* and that
// is the  same which is passed to these methods, remove the CRWWebController
// param from the following methods.
// TODO(crbug.com/380819): Replace the need to use Tab directly here by using a
// delegate pattern.
- (instancetype)initWithSnapshotManager:(SnapshotManager*)snapshotManager
                                    tab:(Tab*)tab;

// If |snapshotCoalescingEnabled| is YES snapshots of the web page are
// coalesced until this method is called with |snapshotCoalescingEnabled| set to
// NO. When snapshot coalescing is enabled, mutiple calls to generate a snapshot
// with the same parameters may be coalesced.
- (void)setSnapshotCoalescingEnabled:(BOOL)snapshotCoalescingEnabled;

// Gets a color snapshot for the WebController's page, calling |callback| if it
// is found. |overlays| is the array of SnapshotOverlay objects (views currently
// overlayed), can be nil.
- (void)retrieveSnapshotForWebController:(CRWWebController*)webController
                               sessionID:(NSString*)sessionID
                            withOverlays:(NSArray*)overlays
                                callback:(void (^)(UIImage* image))callback;

// Gets a grey snapshot for the webController's current page, calling |callback|
// if it is found. |overlays| is the array of SnapshotOverlay objects
// (views currently overlayed), can be nil.
- (void)retrieveGreySnapshotForWebController:(CRWWebController*)webController
                                   sessionID:(NSString*)sessionID
                                withOverlays:(NSArray*)overlays
                                    callback:(void (^)(UIImage* image))callback;

// Invalidates the cached snapshot for the controller's current session and
// forces a more recent snapshot to be generated and stored. Returns the
// snapshot with or without the overlayed views (e.g. infobar, voice search
// button, etc.), and either of the visible frame or of the full screen.
// |overlays| is the array of SnapshotOverlay objects (views currently
// overlayed), can be nil.
- (UIImage*)updateSnapshotForWebController:(CRWWebController*)webController
                                 sessionID:(NSString*)sessionID
                              withOverlays:(NSArray*)overlays
                          visibleFrameOnly:(BOOL)visibleFrameOnly;

// Takes a snapshot image for the current page including optional infobars.
// Returns an autoreleased image cropped and scaled appropriately, with or
// without the overlayed views (e.g. infobar, voice search button, etc.), and
// either of the visible frame or of the full screen.
// Returns nil if a snapshot cannot be generated.
// |overlays| is the array of SnapshotOverlay objects (views currently
// overlayed), can be nil.
- (UIImage*)generateSnapshotForWebController:(CRWWebController*)webController
                                withOverlays:(NSArray*)overlays
                            visibleFrameOnly:(BOOL)visibleFrameOnly;

@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_WEB_CONTROLLER_SNAPSHOT_HELPER_H_
