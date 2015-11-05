// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SCHEDULER_CHILD_WEB_TASK_RUNNER_H_
#define COMPONENTS_SCHEDULER_CHILD_WEB_TASK_RUNNER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/scheduler/scheduler_export.h"
#include "third_party/WebKit/public/platform/WebTaskRunner.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace scheduler {

class SCHEDULER_EXPORT WebTaskRunnerImpl : public blink::WebTaskRunner {
 public:
  explicit WebTaskRunnerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  ~WebTaskRunnerImpl() override;

  const scoped_refptr<base::SingleThreadTaskRunner>& task_runner() const {
    return task_runner_;
  }

  // blink::WebTaskRunner implementation:
  void postTask(const blink::WebTraceLocation& web_location,
                blink::WebTaskRunner::Task* task) override;
  void postDelayedTask(const blink::WebTraceLocation& web_location,
                       blink::WebTaskRunner::Task* task,
                       double delayMs) override;
  blink::WebTaskRunner* clone() override;

  // blink::WebTaskRunner::Task should be wrapped by base::Passed() when
  // used with base::Bind(). See https://crbug.com/551356.
  // runTask() is a helper to call blink::WebTaskRunner::Task::run from
  // scoped_ptr<blink::WebTaskRunner::Task>.
  // runTask() is placed here because scoped_ptr<> cannot be used from Blink.
  static void runTask(scoped_ptr<blink::WebTaskRunner::Task>);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebTaskRunnerImpl);
};

}  // namespace scheduler

#endif  // COMPONENTS_SCHEDULER_CHILD_WEB_TASK_RUNNER_H_
