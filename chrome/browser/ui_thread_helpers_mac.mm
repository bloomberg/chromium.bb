// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui_thread_helpers.h"

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/browser_thread.h"

// This is a wrapper for running Chrome Task objects from within a native run
// loop.  A typical use case is when Chrome work needs to get done but the main
// message loop is blocked by a nested run loop (like an event tracking one).
// This can run specific tasks in that nested loop.  This owns the task and will
// delete it and itself when done.
@interface UITaskHelperMac : NSObject {
 @private
  scoped_ptr<Task> task_;
}
- (id)initWithTask:(Task*)task;
- (void)runTask;
@end

@implementation UITaskHelperMac
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

namespace ui_thread_helpers {

bool PostTaskWhileRunningMenu(const tracked_objects::Location& from_here,
                              Task* task) {
  // This deletes itself and the task after the task runs.
  UITaskHelperMac* runner = [[UITaskHelperMac alloc] initWithTask:task];

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

}  // namespace ui_thread_helpers
