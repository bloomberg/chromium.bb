// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_H_

#import <UIKit/UIKit.h>

#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"
#include "base/time/time.h"
#import "ios/chrome/browser/snapshots/lru_cache.h"

typedef void (^GreyBlock)(UIImage*);

// A singleton providing an in-memory and on-disk cache of tab snapshots.
// A snapshot is a full-screen image of the contents of the page at the current
// scroll offset and zoom level, used to stand in for the UIWebView if it has
// been purged from memory or when quickly switching tabs.
// Persists to disk on a background thread each time a snapshot changes.
@interface SnapshotCache : NSObject {
 @private
  // Dictionary to hold color snapshots in memory. n.b. Color snapshots are not
  // kept in memory on tablets.
  base::scoped_nsobject<NSMutableDictionary> imageDictionary_;

  // Cache to hold color snapshots in memory. n.b. Color snapshots are not
  // kept in memory on tablets. It is used in place of the imageDictionary_ when
  // the LRU cache snapshot experiment is enabled.
  base::scoped_nsobject<LRUCache> lruCache_;

  // Temporary dictionary to hold grey snapshots for tablet side swipe. This
  // will be nil before -createGreyCache is called and after -removeGreyCache
  // is called.
  base::scoped_nsobject<NSMutableDictionary> greyImageDictionary_;
  NSSet* pinnedIDs_;

  // Session ID of most recent pending grey snapshot request.
  base::scoped_nsobject<NSString> mostRecentGreySessionId_;
  // Block used by pending request for a grey snapshot.
  base::scoped_nsprotocol<GreyBlock> mostRecentGreyBlock_;

  // Session ID and correspoinding UIImage for the snapshot that will likely
  // be requested to be saved to disk when the application is backgrounded.
  base::scoped_nsobject<NSString> backgroundingImageSessionId_;
  base::scoped_nsobject<UIImage> backgroundingColorImage_;

  base::mac::ObjCPropertyReleaser propertyReleaser_SnapshotCache_;
}

// Track session IDs to not release on low memory and to reload on
// |UIApplicationDidBecomeActiveNotification|.
@property(nonatomic, retain) NSSet* pinnedIDs;

+ (SnapshotCache*)sharedInstance;

// The scale that should be used for snapshots.
+ (CGFloat)snapshotScaleForDevice;

// Retrieve a cached snapshot for the |sessionID| and return it via the callback
// if it exists. The callback is guaranteed to be called synchronously if the
// image is in memory. It will be called asynchronously if the image is on disk
// or with nil if the image is not present at all.
- (void)retrieveImageForSessionID:(NSString*)sessionID
                         callback:(void (^)(UIImage*))callback;

// Request the session's grey snapshot. If the image is already loaded in
// memory, this will immediately call back on |callback|.
- (void)retrieveGreyImageForSessionID:(NSString*)sessionID
                             callback:(void (^)(UIImage*))callback;

- (void)setImage:(UIImage*)img withSessionID:(NSString*)sessionID;
- (void)removeImageWithSessionID:(NSString*)sessionID;
// Purge the cache of snapshots that are older than |date|. The snapshots for
// the sessions given in |liveSessionIds| will be kept. This will be done
// asynchronously on a background thread.
- (void)purgeCacheOlderThan:(const base::Time&)date
                    keeping:(NSSet*)liveSessionIds;
// Hint that the snapshot for |sessionID| will likely be saved to disk when the
// application is backgrounded.  The snapshot is then saved in memory, so it
// does not need to be read off disk.
- (void)willBeSavedGreyWhenBackgrounding:(NSString*)sessionID;

// Create temporary cache of grey images for tablet side swipe.
- (void)createGreyCache:(NSArray*)sessionIDs;
// Release all images in grey cache.
- (void)removeGreyCache;
// Request the session's grey snapshot. If the image is already loaded this will
// immediately call back on |callback|. Otherwise, only use |callback| for the
// most recent caller. The callback is not guaranteed to be called.
- (void)greyImageForSessionID:(NSString*)sessionID
                     callback:(void (^)(UIImage*))callback;

// Write a grey copy of the snapshot for |sessionID| to disk, but if and only if
// a color version of the snapshot already exists in memory or on disk.
- (void)saveGreyInBackgroundForSessionID:(NSString*)sessionID;
@end

// Additionnal methods that should only be used for tests.
@interface SnapshotCache (TestingAdditions)
- (BOOL)hasImageInMemory:(NSString*)sessionID;
- (BOOL)hasGreyImageInMemory:(NSString*)sessionID;
- (NSUInteger)lruCacheMaxSize;
@end

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_H_
