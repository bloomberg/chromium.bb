// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/task_helpers.h"

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/task.h"

// This is a wrapper for running Task objects from within a native run loop.
// This can run specific tasks in that nested loop.  This owns the task and will
// delete it and itself when done.
@interface NativeTaskRunner : NSObject {
 @private
  scoped_ptr<Task> task_;
}
- (id)initWithTask:(Task*)task;
- (void)runTask;
@end

@implementation NativeTaskRunner
- (id)initWithTask:(Task*)task {
  if ((self = [super init])) {
    task_.reset(task);
  }
  return self;
}

- (void)runTask {
  task_->Run();
  [self autorelease];
}
@end

namespace cocoa_utils {

bool PostTaskInEventTrackingRunLoopMode(
    const tracked_objects::Location& from_here,
    Task* task) {
  // This deletes itself and the task after the task runs.
  NativeTaskRunner* runner = [[NativeTaskRunner alloc] initWithTask:task];

  // Schedule the selector in multiple modes in case this was called while a
  // menu was not running.
  NSArray* modes = [NSArray arrayWithObjects:NSEventTrackingRunLoopMode,
                                             NSDefaultRunLoopMode,
                                             nil];
  [runner performSelectorOnMainThread:@selector(runTask)
                           withObject:nil
                        waitUntilDone:NO
                                modes:modes];
  return true;
}

}  // namespace cocoa_utils
