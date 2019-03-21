// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/staging_watcher.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"

// Best documentation / Is unofficial documentation
//
// Required reading for CFPreferences/NSUserDefaults is at
// <http://dscoder.com/defaults.html>, a post by David "Catfish Man" Smith, who
// re-wrote NSUserDefaults for 10.12 and iPad Classroom support. It is important
// to note that KVO only notifies for changes made by other programs starting
// with that rewrite in 10.12. In macOS 10.11 and earlier, polling is the only
// option. Note that NSUserDefaultsDidChangeNotification never notifies about
// changes made by other programs, not even in 10.12 and later.

namespace {

NSString* const kStagingKey = @"UpdatePending";

}  // namespace

@interface CrStagingKeyWatcher () {
  base::scoped_nsobject<NSUserDefaults> defaults_;
  BOOL observing_;
  base::scoped_nsobject<NSPort> wakePort_;

  BOOL kvoDisabledForTesting_;
  BOOL lastWaitWasBlockedForTesting_;
}

@end

@implementation CrStagingKeyWatcher

- (instancetype)init {
  return [self initWithUserDefaults:[NSUserDefaults standardUserDefaults]];
}

- (instancetype)initWithUserDefaults:(NSUserDefaults*)defaults {
  if ((self = [super init])) {
    defaults_.reset(defaults, base::scoped_policy::RETAIN);
    if (base::mac::IsAtLeastOS10_12() && !kvoDisabledForTesting_) {
      [defaults_ addObserver:self
                  forKeyPath:kStagingKey
                     options:0
                     context:nullptr];
      observing_ = YES;
    }
  }
  return self;
}

- (BOOL)shouldWait {
  NSArray<NSString*>* paths = [defaults_ stringArrayForKey:kStagingKey];
  if (!paths)
    return NO;

  NSString* appPath = [base::mac::OuterBundle() bundlePath];

  return [paths containsObject:appPath];
}

- (void)waitForStagingKey {
  if (![self shouldWait]) {
    lastWaitWasBlockedForTesting_ = NO;
    return;
  }

  NSRunLoop* runloop = [NSRunLoop currentRunLoop];
  if (observing_) {
    wakePort_.reset([NSPort port], base::scoped_policy::RETAIN);
    [runloop addPort:wakePort_ forMode:NSDefaultRunLoopMode];

    while ([self shouldWait] && [runloop runMode:NSDefaultRunLoopMode
                                      beforeDate:[NSDate distantFuture]]) {
      /* run! */
    }
  } else {
    const NSTimeInterval kPollingTime = 0.5;

    while ([self shouldWait] &&
           [runloop
                  runMode:NSDefaultRunLoopMode
               beforeDate:[NSDate dateWithTimeIntervalSinceNow:kPollingTime]]) {
      /* run! */
    }
  }

  lastWaitWasBlockedForTesting_ = YES;
}

- (void)dealloc {
  if (observing_)
    [defaults_ removeObserver:self forKeyPath:kStagingKey context:nullptr];
  if (wakePort_)
    [wakePort_ invalidate];

  [super dealloc];
}

- (void)disableKVOForTesting {
  kvoDisabledForTesting_ = YES;
}

- (BOOL)lastWaitWasBlockedForTesting {
  return lastWaitWasBlockedForTesting_;
}

+ (NSString*)stagingKeyForTesting {
  return kStagingKey;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  [wakePort_ sendBeforeDate:[NSDate date] components:nil from:nil reserved:0];
}

@end
