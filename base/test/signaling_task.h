// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple callback that you can use to wait for events on a thread.

#ifndef BASE_TEST_SIGNALING_TASK_H_
#define BASE_TEST_SIGNALING_TASK_H_
#pragma once

#include "base/task.h"

namespace base {

class WaitableEvent;

class SignalingTask : public Task {
 public:
  explicit SignalingTask(base::WaitableEvent* event);
  virtual ~SignalingTask();

  virtual void Run() OVERRIDE;

 private:
  base::WaitableEvent* event_;
  DISALLOW_COPY_AND_ASSIGN(SignalingTask);
};

}  // namespace base

#endif  // BASE_TEST_SIGNALING_TASK_H_
