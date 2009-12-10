// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Some basic utilities for aiding in the management of Tasks and Callbacks.
// AutoTaskRunner, and its brother AutoCallbackRunner are the scoped_ptr
// equivalents for callbacks.  They are useful for ensuring a callback is
// executed and delete in the face of multiple return points in a function.

#ifndef MEDIA_BASE_CALLBACK_
#define MEDIA_BASE_CALLBACK_

#include "base/scoped_ptr.h"
#include "base/task.h"

namespace media {

class AutoTaskRunner {
 public:
  // Takes ownership of the task.
  explicit AutoTaskRunner(Task* task)
      : task_(task) {
  }

  ~AutoTaskRunner() {
    if (task_.get()) {
      task_->Run();
    }
  }

  Task* release() { return task_.release(); }

 private:
  scoped_ptr<Task> task_;

  DISALLOW_COPY_AND_ASSIGN(AutoTaskRunner);
};

class AutoCallbackRunner {
 public:
  // Takes ownership of the callback.
  explicit AutoCallbackRunner(Callback0::Type* callback)
      : callback_(callback) {
  }

  ~AutoCallbackRunner() {
    if (callback_.get()) {
      callback_->Run();
    }
  }

  Callback0::Type* release() { return callback_.release(); }

 private:
  scoped_ptr<Callback0::Type> callback_;

  DISALLOW_COPY_AND_ASSIGN(AutoCallbackRunner);
};

}  // namespace media

#endif  // MEDIA_BASE_CALLBACK_
