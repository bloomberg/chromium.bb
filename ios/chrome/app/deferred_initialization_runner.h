// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_
#define IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"

// A singleton object to run initialization code asynchronously. Blocks are
// scheduled to be run after a delay. The block is named when added to the
// singleton so that other code can force a deferred block to be run
// synchronously if necessary.
@interface DeferredInitializationRunner : NSObject

// Returns singleton instance.
+ (DeferredInitializationRunner*)sharedInstance;

// Schedules |block| to be run after |delaySeconds| on the current queue.
// This |block| is stored as |name| so code can force this initialization to
// be run synchronously if necessary. This method may be called more than
// once with the same |name| parameter. Any block with the same |name|
// cancels a previously scheduled block of the same |name| if the block has
// not been run yet.
- (void)runBlockNamed:(NSString*)name
                after:(NSTimeInterval)delaySeconds
                block:(ProceduralBlock)block;

// Looks up a previously scheduled block of |name|. If block has not been
// run yet, run it synchronously now.
- (void)runBlockIfNecessary:(NSString*)name;

// Cancels a previously scheduled block of |name|. This is a no-op if the
// block has already been executed.
- (void)cancelBlockNamed:(NSString*)name;

// Number of blocks that have been registered but not executed yet.
// Exposed for testing.
@property(nonatomic, readonly) NSUInteger numberOfBlocksRemaining;

@end

#endif  // IOS_CHROME_APP_DEFERRED_INITIALIZATION_RUNNER_H_
