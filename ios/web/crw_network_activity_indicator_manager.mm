// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/crw_network_activity_indicator_manager.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/threading/thread_checker.h"

@interface CRWNetworkActivityIndicatorManager () {
  base::scoped_nsobject<NSMutableDictionary> _groupCounts;
  NSUInteger _totalCount;
  base::ThreadChecker _threadChecker;
}

@end

@implementation CRWNetworkActivityIndicatorManager

+ (CRWNetworkActivityIndicatorManager*)sharedInstance {
  static CRWNetworkActivityIndicatorManager* instance =
      [[CRWNetworkActivityIndicatorManager alloc] init];
  return instance;
}

- (id)init {
  self = [super init];
  if (self) {
    _groupCounts.reset([[NSMutableDictionary alloc] init]);
    _totalCount = 0;
  }
  return self;
}

- (void)startNetworkTaskForGroup:(NSString*)group {
  [self startNetworkTasks:1 forGroup:group];
}

- (void)stopNetworkTaskForGroup:(NSString*)group {
  [self stopNetworkTasks:1 forGroup:group];
}

- (void)startNetworkTasks:(NSUInteger)numTasks forGroup:(NSString*)group {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(group);
  DCHECK_GT(numTasks, 0U);
  NSUInteger count = 0;
  NSNumber* number = [_groupCounts objectForKey:group];
  if (number) {
    count = [number unsignedIntegerValue];
    DCHECK_GT(count, 0U);
  }
  count += numTasks;
  [_groupCounts setObject:[NSNumber numberWithUnsignedInteger:count]
                   forKey:group];
  _totalCount += numTasks;
  if (_totalCount == numTasks) {
    [[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:YES];
  }
}

- (void)stopNetworkTasks:(NSUInteger)numTasks forGroup:(NSString*)group {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(group);
  DCHECK_GT(numTasks, 0U);
  NSNumber* number = [_groupCounts objectForKey:group];
  DCHECK(number);
  NSUInteger count = [number unsignedIntegerValue];
  DCHECK(count >= numTasks);
  count -= numTasks;
  if (count == 0) {
    [_groupCounts removeObjectForKey:group];
  } else {
    [_groupCounts setObject:[NSNumber numberWithUnsignedInteger:count]
                     forKey:group];
  }
  _totalCount -= numTasks;
  if (_totalCount == 0) {
    [[UIApplication sharedApplication] setNetworkActivityIndicatorVisible:NO];
  }
}

- (NSUInteger)clearNetworkTasksForGroup:(NSString*)group {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(group);
  NSNumber* number = [_groupCounts objectForKey:group];
  if (!number) {
    return 0;
  }
  NSUInteger count = [number unsignedIntegerValue];
  DCHECK_GT(count, 0U);
  [self stopNetworkTasks:count forGroup:group];
  return count;
}

- (NSUInteger)numNetworkTasksForGroup:(NSString*)group {
  DCHECK(_threadChecker.CalledOnValidThread());
  DCHECK(group);
  NSNumber* number = [_groupCounts objectForKey:group];
  if (!number) {
    return 0;
  }
  return [number unsignedIntegerValue];
}

- (NSUInteger)numTotalNetworkTasks {
  DCHECK(_threadChecker.CalledOnValidThread());
  return _totalCount;
}


@end
