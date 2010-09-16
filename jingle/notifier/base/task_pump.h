// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_BASE_TASK_PUMP_H_
#define JINGLE_NOTIFIER_BASE_TASK_PUMP_H_

#include "base/non_thread_safe.h"
#include "base/task.h"
#include "talk/base/taskrunner.h"

namespace notifier {

class TaskPump : public talk_base::TaskRunner {
 public:
  TaskPump();

  virtual ~TaskPump();

  // talk_base::TaskRunner implementation.
  virtual void WakeTasks();
  virtual int64 CurrentTime();

 private:
  void CheckAndRunTasks();

  NonThreadSafe non_thread_safe_;
  ScopedRunnableMethodFactory<TaskPump> scoped_runnable_method_factory_;
  bool posted_wake_;

  DISALLOW_COPY_AND_ASSIGN(TaskPump);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_TASK_PUMP_H_
