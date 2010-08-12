// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple callback that you can use to wait for events on a thread.

#ifndef CHROME_TEST_SIGNALING_TASK_H_
#define CHROME_TEST_SIGNALING_TASK_H_
#pragma once

#include "base/waitable_event.h"

using base::WaitableEvent;

class SignalingTask : public Task {
 public:
  explicit SignalingTask(WaitableEvent* event) : event_(event) {
  }

  virtual void Run() {
    event_->Signal();
  }

 private:
  WaitableEvent* event_;
};

#endif  // CHROME_TEST_SIGNALING_TASK_H_
