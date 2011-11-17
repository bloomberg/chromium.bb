// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_BASE_TASK_PUMP_H_
#define JINGLE_NOTIFIER_BASE_TASK_PUMP_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "base/threading/non_thread_safe.h"
#include "talk/base/taskrunner.h"

namespace notifier {

class TaskPump : public talk_base::TaskRunner {
 public:
  TaskPump();

  virtual ~TaskPump();

  // talk_base::TaskRunner implementation.
  virtual void WakeTasks() OVERRIDE;
  virtual int64 CurrentTime() OVERRIDE;

  // No tasks will be processed after this is called, even if
  // WakeTasks() is called.
  void Stop();

 private:
  void CheckAndRunTasks();

  base::NonThreadSafe non_thread_safe_;
  base::WeakPtrFactory<TaskPump> weak_factory_;
  bool posted_wake_;
  bool stopped_;

  DISALLOW_COPY_AND_ASSIGN(TaskPump);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_TASK_PUMP_H_
