// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_system_resources.h"

#include <cstdlib>
#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"

namespace sync_notifier {

ChromeSystemResources::ChromeSystemResources()
    : scheduler_active_(false) {}

ChromeSystemResources::~ChromeSystemResources() {
  DCHECK(!scheduler_active_);
}

invalidation::Time ChromeSystemResources::current_time() {
  return base::Time::Now();
}

void ChromeSystemResources::StartScheduler() {
  DCHECK(!scheduler_active_);
  scheduler_active_ = true;
}

void ChromeSystemResources::StopScheduler() {
  DCHECK(scheduler_active_);
  scheduler_active_ = false;
}

void ChromeSystemResources::ScheduleWithDelay(
    invalidation::TimeDelta delay,
    invalidation::Closure* task) {
  if (!scheduler_active_) {
    delete task;
    return;
  }
  DCHECK(invalidation::IsCallbackRepeatable(task));
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      NewRunnableFunction(&RunAndDeleteClosure, task),
      delay.InMillisecondsRoundedUp());
}

void ChromeSystemResources::ScheduleImmediately(
    invalidation::Closure* task) {
  if (!scheduler_active_) {
    delete task;
    return;
  }
  DCHECK(invalidation::IsCallbackRepeatable(task));
  MessageLoop::current()->PostTask(
      FROM_HERE, NewRunnableFunction(&RunAndDeleteClosure, task));
}

void ChromeSystemResources::Log(
    LogLevel level, const char* file, int line,
    const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  std::string result;
  StringAppendV(&result, format, ap);
  logging::LogMessage(file, line).stream() << result;
  va_end(ap);
}

}  // namespace sync_notifier
