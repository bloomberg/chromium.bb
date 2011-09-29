// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides some utility classes that help with testing APIs which use
// callbacks.
//
// -- InvokeRunnable --
// The InvokeRunnable is an action that can be used a gMock mock object to
// invoke the Run() method on mock argument.  Example:
//
//   class MockFoo : public Foo {
//    public:
//     MOCK_METHOD0(DoSomething, void(Task* done_cb));
//   };
//
//   EXPECT_CALL(foo, DoSomething(_)).WillOnce(WithArg<0>(InvokeRunnable()));
//
// Then you pass "foo" to something that will eventually call DoSomething().
// The mock action will ensure that passed in done_cb is invoked.
//
//
// -- TaskMocker --
// The TaskMocker class lets you create mock callbacks.  Callbacks are
// difficult to mock because ownership of the callback object is often passed
// to the funciton being invoked.  TaskMocker solves this by providing a
// GetTask() function that creates a new, single-use task that delegates to
// the originating TaskMocker object.  Expectations are placed on the
// originating TaskMocker object.  Each callback retrieved by GetTask() is
// tracked to ensure that it is properly deleted.  The TaskMocker expects to
// outlive all the callbacks retrieved by GetTask().
//
// Example:
//
//   TaskMocker done_cb;
//   EXPECT_CALL(done_cb, Run()).Times(3);
//
//   func1(done_cb.GetTask());
//   func2(done_cb.GetTask());
//   func3(done_cb.GetTask());
//
//   // All 3 callbacks from GetTask() should be deleted before done_cb goes out
//   // of scope.
//
// This class is not threadsafe.
//
// TODO(ajwong): Is it even worth bothering with gmock here?
// TODO(ajwong): Move MockCallback here and merge the implementation
// differences.

#ifndef MEDIA_BASE_MOCK_TASK_H_
#define MEDIA_BASE_MOCK_TASK_H_

#include "base/task.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

ACTION(InvokeRunnable) {
  arg0->Run();
  delete arg0;
}

class TaskMocker {
 public:
  TaskMocker();
  ~TaskMocker();

  Task* CreateTask();

  MOCK_METHOD0(Run, void());

 private:
  friend class CountingTask;
  class CountingTask : public Task {
   public:
    CountingTask(TaskMocker* origin)
        : origin_(origin) {
      origin_->outstanding_tasks_++;
    }

    virtual void Run() {
      origin_->Run();
    }

    virtual ~CountingTask() {
      origin_->outstanding_tasks_--;
    }

   private:
    TaskMocker* origin_;

    DISALLOW_COPY_AND_ASSIGN(CountingTask);
  };

  int outstanding_tasks_;

  DISALLOW_COPY_AND_ASSIGN(TaskMocker);
};

}  // namespace media

#endif  //MEDIA_BASE_MOCK_TASK_H_
