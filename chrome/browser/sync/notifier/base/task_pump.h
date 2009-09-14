// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_TASK_PUMP_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_TASK_PUMP_H_

#include "talk/base/messagequeue.h"
#include "talk/base/taskrunner.h"

namespace notifier {

class TaskPump : public talk_base::MessageHandler,
                 public talk_base::TaskRunner {
 public:
  TaskPump();

  // MessageHandler interface.
  virtual void OnMessage(talk_base::Message* msg);

  // TaskRunner interface.
  virtual void WakeTasks();
  virtual int64 CurrentTime();

 private:
  int timeout_change_count_;
  bool posted_;

  DISALLOW_COPY_AND_ASSIGN(TaskPump);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_TASK_PUMP_H_
