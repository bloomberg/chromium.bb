// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/task_pump.h"
#include "chrome/browser/sync/notifier/base/time.h"
#include "talk/base/common.h"
#include "talk/base/thread.h"

namespace notifier {

// Don't add any messages because there are cleared and thrown away.
enum { MSG_WAKE_UP = 1, MSG_TIMED_WAKE_UP };

TaskPump::TaskPump() : timeout_change_count_(0), posted_(false) {
}

void TaskPump::OnMessage(talk_base::Message* msg) {
  posted_ = false;
  int initial_count = timeout_change_count_;

  // If a task timed out, ensure that it is not blocked, so it will be deleted.
  // This may result in a WakeTasks if a task is timed out.
  PollTasks();

  // Run tasks and handle timeouts.
  RunTasks();
}

void TaskPump::WakeTasks() {
  if (!posted_) {
    // Do the requested wake up.
    talk_base::Thread::Current()->Post(this, MSG_WAKE_UP);
    posted_ = true;
  }
}

int64 TaskPump::CurrentTime() {
  return GetCurrent100NSTime();
}

}  // namespace notifier
