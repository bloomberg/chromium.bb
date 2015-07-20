// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/v8_platform.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/worker_pool.h"
#include "gin/per_isolate_data.h"

namespace gin {

namespace {

base::LazyInstance<V8Platform>::Leaky g_v8_platform = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
V8Platform* V8Platform::Get() { return g_v8_platform.Pointer(); }

V8Platform::V8Platform() {}

V8Platform::~V8Platform() {}

void V8Platform::CallOnBackgroundThread(
    v8::Task* task,
    v8::Platform::ExpectedRuntime expected_runtime) {
  base::WorkerPool::PostTask(
      FROM_HERE,
      base::Bind(&v8::Task::Run, base::Owned(task)),
      expected_runtime == v8::Platform::kLongRunningTask);
}

void V8Platform::CallOnForegroundThread(v8::Isolate* isolate, v8::Task* task) {
  PerIsolateData::From(isolate)->task_runner()->PostTask(
      FROM_HERE, base::Bind(&v8::Task::Run, base::Owned(task)));
}

void V8Platform::CallDelayedOnForegroundThread(v8::Isolate* isolate,
                                               v8::Task* task,
                                               double delay_in_seconds) {
  PerIsolateData::From(isolate)->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&v8::Task::Run, base::Owned(task)),
      base::TimeDelta::FromSecondsD(delay_in_seconds));
}

void V8Platform::CallIdleOnForegroundThread(v8::Isolate* isolate,
                                            v8::IdleTask* task) {
  DCHECK(PerIsolateData::From(isolate)->idle_task_runner());
  PerIsolateData::From(isolate)->idle_task_runner()->PostIdleTask(task);
}

bool V8Platform::IdleTasksEnabled(v8::Isolate* isolate) {
  return PerIsolateData::From(isolate)->idle_task_runner() != nullptr;
}

double V8Platform::MonotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

}  // namespace gin
