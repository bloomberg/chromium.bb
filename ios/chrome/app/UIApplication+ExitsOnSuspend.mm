// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(NDEBUG)

#import "ios/chrome/app/UIApplication+ExitsOnSuspend.h"

#include "base/ios/block_types.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"

NSString* const kExitsOnSuspend = @"EnableExitsOnSuspend";

namespace {
int backgroundTasksCount = 0;
UIBackgroundTaskIdentifier countTaskIdentifier = UIBackgroundTaskInvalid;

// Perform a block on the main thread. Asynchronously if the thread is not the
// main thread, synchronously if the thread is the main thread.
void ExecuteBlockOnMainThread(ProceduralBlock block) {
  if ([NSThread isMainThread])
    block();
  else
    dispatch_async(dispatch_get_main_queue(), block);
}
}  // namespace

// Category defining interposing methods. These methods keep a tally of the
// background tasks count.
@implementation UIApplication (BackgroundTasksCounter)

// Method to replace -beginBackgroundTaskWithExpirationHandler:. The original
// method is called within.
- (UIBackgroundTaskIdentifier)
    cr_interpose_beginBackgroundTaskWithExpirationHandler:
        (ProceduralBlock)handler {
  UIBackgroundTaskIdentifier identifier =
      [self cr_interpose_beginBackgroundTaskWithExpirationHandler:handler];
  if (identifier != UIBackgroundTaskInvalid) {
    ExecuteBlockOnMainThread(^{
      backgroundTasksCount++;
    });
  }
  return identifier;
}

// Method to replace -endBackgroundTask:. The original method is called within.
- (void)cr_interpose_endBackgroundTask:(UIBackgroundTaskIdentifier)identifier {
  if (identifier != UIBackgroundTaskInvalid)
    ExecuteBlockOnMainThread(^{
      backgroundTasksCount--;
    });
  [self cr_interpose_endBackgroundTask:identifier];
}

@end

@interface UIApplication (ExitsOnSuspend_Private)
// Terminate the app immediately. exit(0) is used.
- (void)cr_terminateImmediately;
// Terminate the app via -cr_terminateImmediately when the background tasks
// count is one. The remaining task is the count observation task.
- (void)cr_terminateWhenCountIsOne;
@end

@implementation UIApplication (ExitsOnSuspend)

- (void)cr_terminateWhenDoneWithBackgroundTasks {
  // Add a background task for the count observation.
  DCHECK(countTaskIdentifier == UIBackgroundTaskInvalid);
  countTaskIdentifier = [self beginBackgroundTaskWithExpirationHandler:^{
    // If we get to the end of the 10 minutes, exit.
    [self cr_terminateImmediately];
  }];

  [self cr_terminateWhenCountIsOne];
}

- (void)cr_cancelTermination {
  [NSObject
      cancelPreviousPerformRequestsWithTarget:self
                                     selector:@selector(
                                                  cr_terminateWhenCountIsOne)
                                       object:nil];

  // Cancel the count observation background task.
  [self endBackgroundTask:countTaskIdentifier];
  countTaskIdentifier = UIBackgroundTaskInvalid;
}

#pragma mark - Private

- (void)cr_terminateImmediately {
  DVLOG(1) << "App exited when suspended after running background tasks.";
  // exit(0) will trigger at_exit handlers. Some need to be run on a IOAllowed
  // thread, such as file_util::MemoryMappedFile::CloseHandles().
  base::ThreadRestrictions::SetIOAllowed(true);
  exit(0);
}

- (void)cr_terminateWhenCountIsOne {
  if (backgroundTasksCount <= 1)
    [self cr_terminateImmediately];
  [self performSelector:_cmd withObject:nil afterDelay:1];
}

@end

#endif  // !defined(NDEBUG)
