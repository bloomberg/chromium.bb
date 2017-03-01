// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/alarm_manager.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"

#define MAKE_SURE_OWN_THREAD(callback, ...)                                \
  if (!task_runner_->BelongsToCurrentThread()) {                           \
    task_runner_->PostTask(                                                \
        FROM_HERE, base::Bind(&AlarmManager::callback,                     \
                              weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                \
  }

namespace chromecast {

namespace {
int kClockPollInterval = 5;

void VerifyHandleCallback(const base::Closure& task,
                          base::WeakPtr<AlarmHandle> handle) {
  if (!handle.get()) {
    return;
  }
  task.Run();
}
}  // namespace

AlarmManager::AlarmInfo::AlarmInfo(
    const base::Closure& task,
    base::Time time,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_(task), time_(time), task_runner_(task_runner) {}

AlarmManager::AlarmInfo::~AlarmInfo() {}

AlarmManager::AlarmManager(
    std::unique_ptr<base::Clock> clock,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : clock_(std::move(clock)), task_runner_(task_runner), weak_factory_(this) {
  DCHECK(task_runner.get());
  clock_tick_timer_.reset(new base::RepeatingTimer());
  clock_tick_timer_->SetTaskRunner(task_runner_);
  base::TimeDelta polling_frequency =
      base::TimeDelta::FromSeconds(kClockPollInterval);
  clock_tick_timer_->Start(
      FROM_HERE, polling_frequency,
      base::Bind(&AlarmManager::CheckAlarm, weak_factory_.GetWeakPtr()));
}

AlarmManager::AlarmManager()
    : AlarmManager(base::MakeUnique<base::DefaultClock>(),
                   base::ThreadTaskRunnerHandle::Get()) {}

AlarmManager::~AlarmManager() {}

std::unique_ptr<AlarmHandle> AlarmManager::PostAlarmTask(
    const base::Closure& task,
    base::Time time) {
  std::unique_ptr<AlarmHandle> handle = base::MakeUnique<AlarmHandle>();
  AddAlarm(base::Bind(&VerifyHandleCallback, task, handle->AsWeakPtr()), time,
           base::ThreadTaskRunnerHandle::Get());
  return handle;
}

void AlarmManager::AddAlarm(
    const base::Closure& task,
    base::Time time,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  MAKE_SURE_OWN_THREAD(AddAlarm, task, time, task_runner);
  std::unique_ptr<AlarmInfo> info =
      base::MakeUnique<AlarmInfo>(task, time, task_runner);
  next_alarm_.push(std::move(info));
}

void AlarmManager::CheckAlarm() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  base::Time now = clock_->Now();
  // Fire appropriate alarms.
  while ((!next_alarm_.empty()) && now >= next_alarm_.top()->time()) {
    next_alarm_.top()->task_runner()->PostTask(FROM_HERE,
                                               next_alarm_.top()->task());
    next_alarm_.pop();
  }
}

}  // namespace chromecast
