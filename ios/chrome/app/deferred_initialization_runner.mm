// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/deferred_initialization_runner.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

// An object encapsulating the deferred execution of a block of initialization
// code.
@interface DeferredInitializationBlock : NSObject {
  // A string to reference the initialization block.
  base::scoped_nsobject<NSString> _name;
  // A block of code to execute.
  base::scoped_nsprotocol<ProceduralBlock> _runBlock;
}

// Designated initializer.
- (id)initWithName:(NSString*)name block:(ProceduralBlock)block;

// Dispatches the deferred execution the block after |delaySeconds|.
- (void)dispatch:(NSTimeInterval)delaySeconds;

// Executes the deferred block now.
- (void)runSynchronously;

// Cancels the block's execution.
- (void)cancel;
@end

@implementation DeferredInitializationBlock

// Overrides default designated initializer.
- (id)init {
  NOTREACHED();
  return nil;
}

- (id)initWithName:(NSString*)name block:(ProceduralBlock)block {
  DCHECK(block);
  self = [super init];
  if (self) {
    _name.reset([name copy]);
    _runBlock.reset([block copy]);
  }
  return self;
}

- (void)dispatch:(NSTimeInterval)delaySeconds {
  int64_t nanoseconds = delaySeconds * NSEC_PER_SEC;
  DCHECK([NSThread isMainThread]);
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, nanoseconds),
                 dispatch_get_main_queue(), ^() {
                   [self runSynchronously];
                 });
}

- (void)runSynchronously {
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

@implementation DeferredInitializationRunner {
  base::scoped_nsobject<NSMutableDictionary> _runBlocks;
}

+ (DeferredInitializationRunner*)sharedInstance {
  static DeferredInitializationRunner* instance =
      [[DeferredInitializationRunner alloc] init];
  return instance;
}

- (id)init {
  self = [super init];
  if (self)
    _runBlocks.reset([[NSMutableDictionary dictionary] retain]);
  return self;
}

- (void)runBlockNamed:(NSString*)name
                after:(NSTimeInterval)delaySeconds
                block:(ProceduralBlock)block {
  DCHECK(name);
  // Safety check in case this function is called with a nanosecond or
  // microsecond parameter by mistake.
  DCHECK(delaySeconds < 3600.0);
  // Cancels the previously scheduled block, if there is one, so this
  // |name| block will not be run more than once.
  [[_runBlocks objectForKey:name] cancel];
  base::scoped_nsobject<DeferredInitializationBlock> deferredBlock(
      [[DeferredInitializationBlock alloc] initWithName:name block:block]);
  [_runBlocks setObject:deferredBlock forKey:name];
  [deferredBlock dispatch:delaySeconds];
}

- (void)runBlockIfNecessary:(NSString*)name {
  [[_runBlocks objectForKey:name] runSynchronously];
}

- (void)cancelBlockNamed:(NSString*)name {
  DCHECK(name);
  [[_runBlocks objectForKey:name] cancel];
  [_runBlocks removeObjectForKey:name];
}

- (NSUInteger)numberOfBlocksRemaining {
  return [_runBlocks count];
}

@end
