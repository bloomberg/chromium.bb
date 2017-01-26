// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_cache.h"

#include <unordered_map>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/common/ios_app_bundle_id_prefix.h"
#include "ios/web/public/navigation_item.h"

namespace {
// The maximum amount of pixels the cache should hold.
NSUInteger kCacheMaxPixelCount = 2048 * 1536 * 4;
// Two floats that are different from less than |kMaxFloatDelta| are considered
// equals.
const CGFloat kMaxFloatDelta = 0.01;
}  // namespace

@interface TabSwitcherCache ()
// Clears the cache. Called when a low memory warning was received.
- (void)lowMemoryWarningReceived;
// Returns a autoreleased resized image of |image|.
+ (UIImage*)resizedImage:(UIImage*)image toSize:(CGSize)size;

@end

@implementation TabSwitcherCache {
  base::scoped_nsobject<NSCache> _cache;
  dispatch_queue_t _cacheQueue;
  // The tab models.
  TabModel* _mainTabModel;  // weak
  TabModel* _otrTabModel;   // weak

  // Lock protecting the pending requests map.
  base::Lock _lock;
  std::unordered_map<NSUInteger, PendingSnapshotRequest> _pendingRequests;
}

@synthesize mainTabModel = _mainTabModel;

- (instancetype)init {
  self = [super init];
  if (self) {
    _cache.reset([[NSCache alloc] init]);
    [_cache setTotalCostLimit:kCacheMaxPixelCount];
    NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
    [nc addObserver:self
           selector:@selector(lowMemoryWarningReceived)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
    std::string queueName =
        base::StringPrintf("%s.chrome.ios.TabSwitcherCacheQueue",
                           BUILDFLAG(IOS_APP_BUNDLE_ID_PREFIX));
    _cacheQueue =
        dispatch_queue_create(queueName.c_str(), DISPATCH_QUEUE_SERIAL);
  }
  return self;
}

- (void)dealloc {
  NSNotificationCenter* nc = [NSNotificationCenter defaultCenter];
  [nc removeObserver:self
                name:UIApplicationDidReceiveMemoryWarningNotification
              object:nil];
  dispatch_release(_cacheQueue);
  [_mainTabModel removeObserver:self];
  [_otrTabModel removeObserver:self];
  [super dealloc];
}

- (PendingSnapshotRequest)requestSnapshotForTab:(Tab*)tab
                                       withSize:(CGSize)size
                                completionBlock:
                                    (SnapshotCompletionBlock)completionBlock {
  DCHECK([NSThread isMainThread]);
  DCHECK(tab);
  DCHECK(completionBlock);
  DCHECK(!CGSizeEqualToSize(size, CGSizeZero));
  PendingSnapshotRequest currentRequest;
  UIImage* snapshot = [_cache objectForKey:[self keyForTab:tab]];
  if (snapshot) {
    CGFloat tabContentAreaRatio = tab.snapshotContentArea.size.width /
                                  tab.snapshotContentArea.size.height;
    CGFloat cachedSnapshotRatio =
        [snapshot size].width / [snapshot size].height;
    // Check that the cached snapshot's ratio matches the content area ratio.
    if (std::abs(tabContentAreaRatio - cachedSnapshotRatio) < kMaxFloatDelta &&
        [snapshot size].width >= size.width) {
      // Cache hit.
      completionBlock(snapshot);
      return currentRequest;
    }
  }

  // Cache miss.
  currentRequest = [self recordPendingRequestForTab:tab];
  NSString* key = [self keyForTab:tab];
  [tab retrieveSnapshot:^(UIImage* snapshot) {
    PendingSnapshotRequest requestForSession = [self pendingRequestForTab:tab];
    // Cancel this request if another one has replaced it for this sessionId.
    if (currentRequest.requestId != requestForSession.requestId)
      return;
    dispatch_async(_cacheQueue, ^{
      DCHECK(![NSThread isMainThread]);
      UIImage* resizedSnapshot =
          [TabSwitcherCache resizedImage:snapshot toSize:size];
      if ([self storeImage:resizedSnapshot forKey:key request:currentRequest]) {
        dispatch_async(dispatch_get_main_queue(), ^{
          // Cancel this request if another one has replaced it for this
          // sessionId.
          PendingSnapshotRequest requestForSession =
              [self pendingRequestForTab:tab];
          if (currentRequest.requestId != requestForSession.requestId)
            return;
          completionBlock(resizedSnapshot);
          [self removePendingSnapshotRequest:currentRequest];
        });
      }
    });
  }];
  return currentRequest;
}

- (void)updateSnapshotForTab:(Tab*)tab
                   withImage:(UIImage*)image
                        size:(CGSize)size {
  DCHECK([NSThread isMainThread]);
  DCHECK(tab);
  DCHECK(image);
  PendingSnapshotRequest currentRequest = [self recordPendingRequestForTab:tab];
  NSString* key = [self keyForTab:tab];

  dispatch_async(_cacheQueue, ^{
    DCHECK(![NSThread isMainThread]);
    UIImage* resizedSnapshot =
        [TabSwitcherCache resizedImage:image toSize:size];
    [self storeImage:resizedSnapshot forKey:key request:currentRequest];
    [self removePendingSnapshotRequest:currentRequest];
  });
}

- (void)cancelPendingSnapshotRequest:(PendingSnapshotRequest)pendingRequest {
  [self removePendingSnapshotRequest:pendingRequest];
}

#pragma mark - Private

- (NSString*)keyForTab:(Tab*)tab {
  DCHECK([NSThread isMainThread]);
  return tab.tabId;
}

- (PendingSnapshotRequest)recordPendingRequestForTab:(Tab*)tab {
  PendingSnapshotRequest pendingRequest;
  pendingRequest.requestId = [[NSDate date] timeIntervalSince1970];
  pendingRequest.sessionId = [[self keyForTab:tab] hash];
  base::AutoLock guard(_lock);
  _pendingRequests[pendingRequest.sessionId] = pendingRequest;
  return pendingRequest;
}

- (PendingSnapshotRequest)pendingRequestForTab:(Tab*)tab {
  DCHECK([NSThread isMainThread]);
  PendingSnapshotRequest pendingRequest;
  if (!tab.webState)
    return pendingRequest;
  NSUInteger sessionId = [[self keyForTab:tab] hash];
  base::AutoLock guard(_lock);
  auto it = _pendingRequests.find(sessionId);
  if (it != _pendingRequests.end())
    pendingRequest = it->second;
  return pendingRequest;
}

- (void)removePendingSnapshotRequest:(PendingSnapshotRequest)pendingRequest {
  base::AutoLock guard(_lock);
  auto itRequest = _pendingRequests.find(pendingRequest.sessionId);
  if (itRequest != _pendingRequests.end() &&
      pendingRequest.requestId == itRequest->second.requestId) {
    _pendingRequests.erase(itRequest);
  }
}

- (void)removePendingSnapshotRequestForTab:(Tab*)tab {
  base::AutoLock guard(_lock);
  auto itRequest = _pendingRequests.find([[self keyForTab:tab] hash]);
  if (itRequest != _pendingRequests.end())
    _pendingRequests.erase(itRequest);
}

- (BOOL)storeImage:(UIImage*)image
            forKey:(NSString*)key
           request:(PendingSnapshotRequest)request {
  DCHECK(request.requestId != 0);
  if (!image)
    return NO;

  {
    base::AutoLock guard(_lock);
    auto it = _pendingRequests.find(request.sessionId);
    if (it == _pendingRequests.end())
      return NO;

    // Only write the image in cache if the request is still valid.
    if (request.requestId != it->second.requestId)
      return NO;
  }

  const CGFloat screenScale = [[UIScreen mainScreen] scale];
  const NSUInteger cost =
      image.size.width * screenScale * image.size.height * screenScale;
  [_cache setObject:image forKey:key cost:cost];
  return YES;
}

+ (UIImage*)resizedImage:(UIImage*)image toSize:(CGSize)size {
  DCHECK(image.scale == 1);
  CGFloat screenScale = [[UIScreen mainScreen] scale];
  CGSize pixelSize = size;
  pixelSize.width *= screenScale;
  pixelSize.height *= screenScale;
  UIImage* resizedSnapshot =
      ResizeImage(image, pixelSize, ProjectionMode::kAspectFillNoClipping, YES);
  // Creates a new image with the correct |scale| attribute.
  return [[[UIImage alloc] initWithCGImage:resizedSnapshot.CGImage
                                     scale:screenScale
                               orientation:UIImageOrientationUp] autorelease];
}

- (void)lowMemoryWarningReceived {
  [_cache removeAllObjects];
}

- (void)setMainTabModel:(TabModel*)mainTabModel
            otrTabModel:(TabModel*)otrTabModel {
  if (mainTabModel != _mainTabModel)
    [self replaceOldTabModel:&_mainTabModel withTabModel:mainTabModel];
  if (otrTabModel != _otrTabModel)
    [self replaceOldTabModel:&_otrTabModel withTabModel:otrTabModel];
}

- (void)replaceOldTabModel:(TabModel**)oldTabModel
              withTabModel:(TabModel*)newTabModel {
  [*oldTabModel removeObserver:self];
  *oldTabModel = newTabModel;
  [newTabModel addObserver:self];
}

#pragma mark - TabModelObserver

- (void)tabModel:(TabModel*)model
    didRemoveTab:(Tab*)tab
         atIndex:(NSUInteger)index {
  [self removePendingSnapshotRequestForTab:tab];
  [_cache removeObjectForKey:[self keyForTab:tab]];
}

- (void)tabModel:(TabModel*)model
    didChangeTabSnapshot:(Tab*)tab
               withImage:image {
  [self removePendingSnapshotRequestForTab:tab];
  [_cache removeObjectForKey:[self keyForTab:tab]];
}

@end
