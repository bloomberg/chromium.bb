// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TEST_RUNNER_WEB_TASK_H_
#define COMPONENTS_TEST_RUNNER_WEB_TASK_H_

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebTaskRunner.h"

namespace test_runner {

// blink::WebTaskRunner::Task that wraps a base::Closure.
class WebCallbackTask : public blink::WebTaskRunner::Task {
 public:
  WebCallbackTask(const base::Closure& callback);
  ~WebCallbackTask() override;

  void run() override;

 private:
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(WebCallbackTask);
};

}  // namespace test_runner

#endif  // COMPONENTS_TEST_RUNNER_WEB_TASK_H_
