// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_system_resources.h"

#include <cstdlib>
#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"

namespace sync_notifier {

ChromeSystemResources::ChromeSystemResources() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

ChromeSystemResources::~ChromeSystemResources() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  StopScheduler();
}

invalidation::Time ChromeSystemResources::current_time() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return base::Time::Now();
}

void ChromeSystemResources::StartScheduler() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  scoped_runnable_method_factory_.reset(
      new ScopedRunnableMethodFactory<ChromeSystemResources>(this));
}

void ChromeSystemResources::StopScheduler() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  scoped_runnable_method_factory_.reset();
  STLDeleteElements(&posted_tasks_);
}

void ChromeSystemResources::ScheduleWithDelay(
    invalidation::TimeDelta delay,
    invalidation::Closure* task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Task* task_to_post = MakeTaskToPost(task);
  if (!task_to_post) {
    return;
  }
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE, task_to_post, delay.InMillisecondsRoundedUp());
}

void ChromeSystemResources::ScheduleImmediately(
    invalidation::Closure* task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  Task* task_to_post = MakeTaskToPost(task);
  if (!task_to_post) {
    return;
  }
  MessageLoop::current()->PostTask(FROM_HERE, task_to_post);
}

void ChromeSystemResources::Log(
    LogLevel level, const char* file, int line,
    const char* format, ...) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  va_list ap;
  va_start(ap, format);
  std::string result;
  StringAppendV(&result, format, ap);
  logging::LogMessage(file, line).stream() << result;
  va_end(ap);
}

Task* ChromeSystemResources::MakeTaskToPost(
    invalidation::Closure* task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  DCHECK(invalidation::IsCallbackRepeatable(task));
  if (!scoped_runnable_method_factory_.get()) {
    delete task;
    return NULL;
  }
  posted_tasks_.insert(task);
  Task* task_to_post =
      scoped_runnable_method_factory_->NewRunnableMethod(
          &ChromeSystemResources::RunPostedTask, task);
  return task_to_post;
}

void ChromeSystemResources::RunPostedTask(invalidation::Closure* task) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  RunAndDeleteClosure(task);
  posted_tasks_.erase(task);
}

}  // namespace sync_notifier
