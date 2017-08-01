// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_MANAGER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_MANAGER_H_

#import <UIKit/UIKit.h>

namespace web {
class WebState;
}

// Snapshot manager for contents of a tab. A snapshot is a full-screen image
// of the contents of the page at the current scroll offset and zoom level,
// used to stand in for the web view if it has been purged from memory or when
// quickly switching tabs. Uses |SnapshotCache| to cache (and persist)
// snapshots.
//
// The snapshots are identified by a "session id" which is unique per tab. This
// allows quick identification and replacement as a tab changes pages.
@interface SnapshotManager : NSObject

// Designated initializer. The |webState| must not be null.
- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Takes a snapshot for the supplied view (which should correspond to the given
// type of web view). Returns an autoreleased image cropped and scaled
// appropriately.
// The image is not yet cached.
// The image can also contain overlays (if |overlays| is not nil and not empty).
- (UIImage*)generateSnapshotForView:(UIView*)view
                           withRect:(CGRect)rect
                           overlays:(NSArray*)overlays;

// Retrieve a cached snapshot for the |sessionID| and return it via the callback
// if it exists. The callback is guaranteed to be called synchronously if the
// image is in memory. It will be called asynchronously if the image is on disk
// or with nil if the image is not present at all.
- (void)retrieveImageForSessionID:(NSString*)sessionID
                         callback:(void (^)(UIImage*))callback;

// Request the session's grey snapshot. If the image is already loaded in
// memory, this will immediately call back on |callback|.  Otherwise, the grey
// image will be loaded off disk or created by converting an existing color
// snapshot to grey.
- (void)retrieveGreyImageForSessionID:(NSString*)sessionID
                             callback:(void (^)(UIImage*))callback;

// Stores the supplied thumbnail for the specified |sessionID|.
- (void)setImage:(UIImage*)image withSessionID:(NSString*)sessionID;

// Removes the cached thumbnail for the specified |sessionID|.
- (void)removeImageWithSessionID:(NSString*)sessionID;

// Request the grey image from the in-memory cache only.
- (void)greyImageForSessionID:(NSString*)sessionID
                     callback:(void (^)(UIImage*))callback;
@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_MANAGER_H_
