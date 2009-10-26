// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/worker_pool_mac.h"

#include "base/logging.h"
#import "base/scoped_nsautorelease_pool.h"
#include "base/scoped_ptr.h"
#import "base/singleton_objc.h"
#include "base/task.h"

@implementation WorkerPoolObjC

+ (NSOperationQueue*)sharedOperationQueue {
  return SingletonObjC<NSOperationQueue>::get();
}

@end  // @implementation WorkerPoolObjC

// TaskOperation adapts Task->Run() for use in an NSOperationQueue.
@interface TaskOperation : NSOperation {
 @private
  scoped_ptr<Task> task_;
}

// Returns an autoreleased instance of TaskOperation.  See -initWithTask: for
// details.
+ (id)taskOperationWithTask:(Task*)task;

// Designated initializer.  |task| is adopted as the Task* whose Run method
// this operation will call when executed.
- (id)initWithTask:(Task*)task;

@end  // @interface TaskOperation

@implementation TaskOperation

+ (id)taskOperationWithTask:(Task*)task {
  return [[[TaskOperation alloc] initWithTask:task] autorelease];
}

- (id)init {
  return [self initWithTask:NULL];
}

- (id)initWithTask:(Task*)task {
  if ((self = [super init])) {
    task_.reset(task);
  }
  return self;
}

- (void)main {
  DCHECK(task_.get()) << "-[TaskOperation main] called with no task";
  if (!task_.get()) {
    return;
  }

  base::ScopedNSAutoreleasePool autoreleasePool;

  task_->Run();
  task_.reset(NULL);
}

- (void)dealloc {
  DCHECK(!task_.get())
      << "-[TaskOperation dealloc] called without running task";

  [super dealloc];
}

@end  // @implementation TaskOperation

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          Task* task, bool task_is_slow) {
  // Ignore |task_is_slow|, it doesn't map directly to any tunable aspect of
  // an NSOperation.

  DCHECK(task) << "WorkerPool::PostTask called with no task";
  if (!task) {
    return false;
  }

  task->SetBirthPlace(from_here);

  NSOperationQueue* operation_queue = [WorkerPoolObjC sharedOperationQueue];
  [operation_queue addOperation:[TaskOperation taskOperationWithTask:task]];

  return true;
}
