// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/staging_watcher.h"

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_block.h"
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
  NSTimeInterval pollingTime_;
  base::scoped_nsobject<NSTimer> pollingTimer_;
  BOOL observing_;
  base::mac::ScopedBlock<StagingKeyChangedObserver> callback_;
  BOOL lastStagingKeyValue_;

  BOOL kvoDisabledForTesting_;
  BOOL lastWaitWasBlockedForTesting_;
}

@end

@implementation CrStagingKeyWatcher

- (instancetype)initWithPollingTime:(NSTimeInterval)pollingTime {
  return [self initWithUserDefaults:[NSUserDefaults standardUserDefaults]
                        pollingTime:pollingTime];
}

- (instancetype)initWithUserDefaults:(NSUserDefaults*)defaults
                         pollingTime:(NSTimeInterval)pollingTime {
  if ((self = [super init])) {
    pollingTime_ = pollingTime;
    defaults_.reset(defaults, base::scoped_policy::RETAIN);
    lastStagingKeyValue_ = [self isStagingKeySet];
    if (base::mac::IsAtLeastOS10_12() && !kvoDisabledForTesting_) {
      // If a change is made in another process (which is the use case here),
      // the prior value is never provided in the observation callback change
      // dictionary, whether or not NSKeyValueObservingOptionPrior is specified.
      // Therefore, pass in 0 for the NSKeyValueObservingOptions and rely on
      // keeping the previous value in |lastStagingKeyValue_|.
      [defaults_ addObserver:self
                  forKeyPath:kStagingKey
                     options:0
                     context:nullptr];
      observing_ = YES;
    }
  }
  return self;
}

- (BOOL)isStagingKeySet {
  NSArray<NSString*>* paths = [defaults_ stringArrayForKey:kStagingKey];
  if (!paths)
    return NO;

  NSString* appPath = [base::mac::OuterBundle() bundlePath];

  return [paths containsObject:appPath];
}

- (void)waitForStagingKeyToClear {
  if (![self isStagingKeySet]) {
    lastWaitWasBlockedForTesting_ = NO;
    return;
  }

  NSRunLoop* runloop = [NSRunLoop currentRunLoop];
  if (observing_) {
    callback_.reset(
        ^(BOOL stagingKeySet) {
          CFRunLoopStop([runloop getCFRunLoop]);
        },
        base::scoped_policy::RETAIN);

    while ([self isStagingKeySet] && [runloop runMode:NSDefaultRunLoopMode
                                           beforeDate:[NSDate distantFuture]]) {
      /* run! */
    }
  } else {
    while ([self isStagingKeySet] &&
           [runloop
                  runMode:NSDefaultRunLoopMode
               beforeDate:[NSDate dateWithTimeIntervalSinceNow:pollingTime_]]) {
      /* run! */
    }
  }

  lastWaitWasBlockedForTesting_ = YES;
}

- (void)setStagingKeyChangedObserver:(StagingKeyChangedObserver)block {
  callback_.reset(block, base::scoped_policy::RETAIN);

  if (observing_) {
    // Nothing to be done; the observation is already started.
  } else {
    pollingTimer_.reset(
        [NSTimer scheduledTimerWithTimeInterval:pollingTime_
                                         target:self
                                       selector:@selector(timerFired:)
                                       userInfo:nil
                                        repeats:YES],
        base::scoped_policy::RETAIN);
  }
}

- (void)timerFired:(NSTimer*)timer {
  [self observeValueForKeyPath:nil ofObject:nil change:nil context:nil];
}

- (void)dealloc {
  if (observing_)
    [defaults_ removeObserver:self forKeyPath:kStagingKey context:nullptr];
  if (pollingTimer_)
    [pollingTimer_ invalidate];

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
  BOOL isStagingKeySet = [self isStagingKeySet];
  if (isStagingKeySet == lastStagingKeyValue_)
    return;

  lastStagingKeyValue_ = isStagingKeySet;
  callback_.get()([self isStagingKeySet]);
}

@end
