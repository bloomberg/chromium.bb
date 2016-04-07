// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_task.h"

namespace test_runner {

WebCallbackTask::WebCallbackTask(const base::Closure& callback)
    : callback_(callback) {}

WebCallbackTask::~WebCallbackTask() {}

void WebCallbackTask::run() {
  callback_.Run();
}

}  // namespace test_runner
