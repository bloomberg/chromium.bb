// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_TEST_TASK_RUNNER_H_
#define CHROME_BROWSER_POLICY_TEST_TASK_RUNNER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class TestTaskRunner : public base::TaskRunner {
 public:
  TestTaskRunner();

  virtual bool RunsTasksOnCurrentThread() const OVERRIDE { return true; }
  MOCK_METHOD3(PostDelayedTask, bool(const tracked_objects::Location&,
                                     const base::Closure&,
                                     base::TimeDelta));

 protected:
  virtual ~TestTaskRunner();

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTaskRunner);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_TEST_TASK_RUNNER_H_
