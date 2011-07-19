// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/callback.h"

namespace media {

AutoCallbackRunner::~AutoCallbackRunner() {
  if (callback_.get()) {
    callback_->Run();
  }
}

Callback0::Type* TaskToCallbackAdapter::NewCallback(Task* task) {
  return new TaskToCallbackAdapter(task);
}

TaskToCallbackAdapter::~TaskToCallbackAdapter() {}

void TaskToCallbackAdapter::RunWithParams(const Tuple0& params) {
  task_->Run();
}

TaskToCallbackAdapter::TaskToCallbackAdapter(Task* task) : task_(task) {}

}  // namespace media
