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


// Helper class used to implement NewCallbackForClosure(). It simply
// holds onto the Closure until Run() is called. When Run() is called,
// the closure is run and this object deletes itself.
//
// TODO(acolwell): Delete this once all old style callbacks have been
// removed from the media code.
class RunClosureAndDeleteHelper {
 public:
  explicit RunClosureAndDeleteHelper(const base::Closure& cb)
      : cb_(cb) {
    DCHECK(!cb_.is_null());
  }

  void Run() {
    cb_.Run();
    delete this;
  }

 private:
  ~RunClosureAndDeleteHelper() {}

  base::Closure cb_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RunClosureAndDeleteHelper);
};

// TODO(acolwell): Delete this once all old style callbacks have been
// removed from the media code.
Callback0::Type* NewCallbackForClosure(const base::Closure& cb) {
  return NewCallback(new RunClosureAndDeleteHelper(cb),
                     &RunClosureAndDeleteHelper::Run);
}

}  // namespace media
