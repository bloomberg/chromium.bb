// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_VIRTUAL_TIME_TQM_DELEGATE_H_
#define COMPONENTS_SCHEDULER_CHILD_VIRTUAL_TIME_TQM_DELEGATE_H_

#include <map>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/time/tick_clock.h"
#include "components/scheduler/child/scheduler_tqm_delegate.h"
#include "components/scheduler/scheduler_export.h"

namespace scheduler {

class SCHEDULER_EXPORT VirtualTimeTqmDelegate : public SchedulerTqmDelegate {
 public:
  // |message_loop| is not owned and must outlive the lifetime of this object.
  static scoped_refptr<VirtualTimeTqmDelegate> Create(
      base::MessageLoop* message_loop,
      base::TimeTicks initial_now);

  // SchedulerTqmDelegate implementation
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void RestoreDefaultTaskRunner() override;
  bool PostDelayedTask(const tracked_objects::Location& from_here,
                       const base::Closure& task,
                       base::TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const tracked_objects::Location& from_here,
                                  const base::Closure& task,
                                  base::TimeDelta delay) override;
  bool RunsTasksOnCurrentThread() const override;
  bool IsNested() const override;
  base::TimeTicks NowTicks() override;
  void OnNoMoreImmediateWork() override;
  double CurrentTimeSeconds() const override;

 protected:
  ~VirtualTimeTqmDelegate() override;

 private:
  explicit VirtualTimeTqmDelegate(base::MessageLoop* message_loop,
                                  base::TimeTicks initial_no);

  void AdvancedTimeTo(base::TimeTicks now);

  typedef std::multimap<base::TimeTicks, base::Closure> DelayedWakeupMultimap;

  DelayedWakeupMultimap delayed_wakeup_multimap_;

  // Not owned.
  base::MessageLoop* message_loop_;
  scoped_refptr<SingleThreadTaskRunner> message_loop_task_runner_;
  base::TimeTicks now_;

  DISALLOW_COPY_AND_ASSIGN(VirtualTimeTqmDelegate);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_VIRTUAL_TIME_TQM_DELEGATE_H_
