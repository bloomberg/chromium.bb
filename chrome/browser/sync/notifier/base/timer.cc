// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/timer.h"

namespace notifier {

Timer::Timer(talk_base::Task* parent, int timeout_seconds, bool repeat)
  : Task(parent),
    repeat_(repeat) {

  set_timeout_seconds(timeout_seconds);
  Start();
  ResumeTimeout();
}

Timer::~Timer() {
}

int Timer::OnTimeout() {
  if (!repeat_) {
    return STATE_DONE;
  }
  ResetTimeout();
  return STATE_BLOCKED;
}

int Timer::ProcessStart() {
  return STATE_BLOCKED;
}

}  // namespace notifier
