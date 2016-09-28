// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/deferred_initialization_runner.h"

#include <stdint.h>

#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"

// An object encapsulating the deferred execution of a block of initialization
// code.
@interface DeferredInitializationBlock : NSObject {
  // A string to reference the initialization block.
  base::scoped_nsobject<NSString> _name;
  // A block of code to execute.
  base::mac::ScopedBlock<ProceduralBlock> _runBlock;
}

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithName:(NSString*)name
                       block:(ProceduralBlock)block NS_DESIGNATED_INITIALIZER;

// Executes the deferred block now.
- (void)run;

// Cancels the block's execution.
- (void)cancel;

@end

@implementation DeferredInitializationBlock

// Overrides default designated initializer.
- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithName:(NSString*)name block:(ProceduralBlock)block {
  DCHECK(block);
  self = [super init];
  if (self) {
    _name.reset([name copy]);
    _runBlock.reset(block, base::scoped_policy::RETAIN);
  }
  return self;
}

- (void)run {
  DCHECK([NSThread isMainThread]);
  ProceduralBlock deferredBlock = _runBlock.get();
  if (!deferredBlock)
    return;
  deferredBlock();
  [[DeferredInitializationRunner sharedInstance] cancelBlockNamed:_name];
}

- (void)cancel {
  _runBlock.reset();
}

@end

@interface DeferredInitializationRunner () {
  base::scoped_nsobject<NSMutableArray> _blocksNameQueue;
  base::scoped_nsobject<NSMutableDictionary> _runBlocks;
  BOOL _isBlockScheduled;
}

// Schedule the next block to be run after |delay| it will automatically
// schedule the next block after |delayBetweenBlocks|.
- (void)scheduleNextBlockWithDelay:(NSTimeInterval)delay;

// Time interval between two blocks. Default value is 200ms.
@property(nonatomic) NSTimeInterval delayBetweenBlocks;

// Time interval before running the first block. Default value is 3s.
@property(nonatomic) NSTimeInterval delayBeforeFirstBlock;

@end

@implementation DeferredInitializationRunner

@synthesize delayBetweenBlocks = _delayBetweenBlocks;
@synthesize delayBeforeFirstBlock = _delayBeforeFirstBlock;

+ (DeferredInitializationRunner*)sharedInstance {
  static dispatch_once_t once = 0;
  static DeferredInitializationRunner* instance = nil;
  dispatch_once(&once, ^{
    instance = [[DeferredInitializationRunner alloc] init];
  });
  return instance;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _blocksNameQueue.reset([[NSMutableArray array] retain]);
    _runBlocks.reset([[NSMutableDictionary dictionary] retain]);
    _isBlockScheduled = NO;
    _delayBetweenBlocks = 0.2;
    _delayBeforeFirstBlock = 3.0;
  }
  return self;
}

- (void)enqueueBlockNamed:(NSString*)name block:(ProceduralBlock)block {
  DCHECK(name);
  DCHECK([NSThread isMainThread]);
  [self cancelBlockNamed:name];
  [_blocksNameQueue addObject:name];

  base::scoped_nsobject<DeferredInitializationBlock> deferredBlock(
      [[DeferredInitializationBlock alloc] initWithName:name block:block]);
  [_runBlocks setObject:deferredBlock forKey:name];

  if (!_isBlockScheduled) {
    [self scheduleNextBlockWithDelay:self.delayBeforeFirstBlock];
  }
}

- (void)scheduleNextBlockWithDelay:(NSTimeInterval)delay {
  DCHECK([NSThread isMainThread]);
  _isBlockScheduled = NO;
  NSString* nextBlockName = [_blocksNameQueue firstObject];
  if (!nextBlockName)
    return;

  DeferredInitializationBlock* nextBlock =
      [_runBlocks objectForKey:nextBlockName];
  DCHECK(nextBlock);

  base::WeakNSObject<DeferredInitializationRunner> weakSelf(self);

  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delay * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        [nextBlock run];
        [weakSelf scheduleNextBlockWithDelay:[weakSelf delayBetweenBlocks]];
      });

  _isBlockScheduled = YES;
  [_blocksNameQueue removeObjectAtIndex:0];
}

- (void)runBlockIfNecessary:(NSString*)name {
  DCHECK([NSThread isMainThread]);
  [[_runBlocks objectForKey:name] run];
}

- (void)cancelBlockNamed:(NSString*)name {
  DCHECK([NSThread isMainThread]);
  DCHECK(name);
  [_blocksNameQueue removeObject:name];
  [[_runBlocks objectForKey:name] cancel];
  [_runBlocks removeObjectForKey:name];
}

- (NSUInteger)numberOfBlocksRemaining {
  return [_runBlocks count];
}

@end
