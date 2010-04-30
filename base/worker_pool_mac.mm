// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/worker_pool_mac.h"

#include "base/histogram.h"
#include "base/logging.h"
#import "base/scoped_nsautorelease_pool.h"
#include "base/scoped_ptr.h"
#import "base/singleton_objc.h"
#include "base/task.h"

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace {

Lock lock_;
base::Time last_check_;            // Last hung-test check.
std::vector<id> outstanding_ops_;  // Outstanding operations at last check.
size_t running_ = 0;               // Operations in |Run()|.
size_t outstanding_ = 0;           // Operations posted but not completed.

}  // namespace

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

  {
    AutoLock locked(lock_);
    ++running_;
  }

  base::ScopedNSAutoreleasePool autoreleasePool;

  @try {
    task_->Run();
  } @catch(NSException* exception) {
    LOG(ERROR) << "-[TaskOperation main] caught an NSException: "
               << [[exception description] UTF8String];
  } @catch(id exception) {
    LOG(ERROR) << "-[TaskOperation main] caught an unknown exception";
  }

  task_.reset(NULL);

  {
    AutoLock locked(lock_);
    --running_;
    --outstanding_;
  }
}

- (void)dealloc {
  DCHECK(!task_.get())
      << "-[TaskOperation dealloc] called without running task";

  [super dealloc];
}

@end  // @implementation TaskOperation

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          Task* task, bool task_is_slow) {
  base::ScopedNSAutoreleasePool autorelease_pool;

  // Ignore |task_is_slow|, it doesn't map directly to any tunable aspect of
  // an NSOperation.

  DCHECK(task) << "WorkerPool::PostTask called with no task";
  if (!task) {
    return false;
  }

  task->SetBirthPlace(from_here);

  NSOperationQueue* operation_queue = [WorkerPoolObjC sharedOperationQueue];
  [operation_queue addOperation:[TaskOperation taskOperationWithTask:task]];

  if ([operation_queue isSuspended]) {
    LOG(WARNING) << "WorkerPool::PostTask freeing stuck NSOperationQueue";

    // Nothing should ever be suspending this queue, but in case it winds up
    // happening, free things up.  This is a purely speculative shot in the
    // dark for http://crbug.com/20471.
    [operation_queue setSuspended:NO];
  }

  // Periodically calculate the set of operations which have not made
  // progress and report how many there are.  This should provide a
  // sense of how many clients are seeing hung operations of any sort,
  // and a sense of how many clients are seeing "too many" hung
  // operations.
  std::vector<id> hung_ops;
  size_t outstanding_delta = 0;
  size_t running_ops = 0;
  {
    const base::TimeDelta kCheckPeriod(base::TimeDelta::FromMinutes(10));
    base::Time now = base::Time::Now();

    AutoLock locked(lock_);
    ++outstanding_;
    running_ops = running_;
    if (last_check_.is_null() || now - last_check_ > kCheckPeriod) {
      base::ScopedNSAutoreleasePool autoreleasePool;
      std::vector<id> ops;
      for (id op in [operation_queue operations]) {
        // DO NOT RETAIN.
        ops.push_back(op);
      }
      std::sort(ops.begin(), ops.end());

      outstanding_delta = outstanding_ - ops.size();

      std::set_intersection(outstanding_ops_.begin(), outstanding_ops_.end(),
                            ops.begin(), ops.end(),
                            std::back_inserter(hung_ops));

      outstanding_ops_.swap(ops);
      last_check_ = now;
    }
  }

  // Don't report "nothing to report".
  const size_t kUnaccountedOpsDelta = 10;
  if (hung_ops.size() > 0 || outstanding_delta > kUnaccountedOpsDelta) {
    UMA_HISTOGRAM_COUNTS_100("OSX.HungWorkers", hung_ops.size());
    UMA_HISTOGRAM_COUNTS_100("OSX.OutstandingDelta", outstanding_delta);
    UMA_HISTOGRAM_COUNTS_100("OSX.RunningOps", running_ops);
  }

  return true;
}
