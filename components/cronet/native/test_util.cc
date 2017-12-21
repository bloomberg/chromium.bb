// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/test_util.h"

#include "base/bind.h"
#include "base/task_scheduler/post_task.h"

namespace {
// Implementation of PostTaskExecutor methods.
void TestExecutor_Execute(Cronet_ExecutorPtr self, Cronet_RunnablePtr command) {
  CHECK(self);
  DVLOG(1) << "Post Task";
  base::PostTask(FROM_HERE, base::BindOnce(
                                [](Cronet_RunnablePtr command) {
                                  Cronet_Runnable_Run(command);
                                  Cronet_Runnable_Destroy(command);
                                },
                                command));
}
}  // namespace

namespace cronet {
namespace test {

Cronet_ExecutorPtr CreateTestExecutor() {
  return Cronet_Executor_CreateStub(TestExecutor_Execute);
}

}  // namespace test
}  // namespace cronet
