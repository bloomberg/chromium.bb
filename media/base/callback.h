// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

// Some basic utilities for aiding in the management of Tasks and Callbacks.
//
// AutoTaskRunner, and its brother AutoCallbackRunner are the scoped_ptr
// equivalents for callbacks.  They are useful for ensuring a callback is
// executed and delete in the face of multiple return points in a function.
//
// TaskToCallbackAdapter converts a Task to a Callback0::Type since the two type
// heirarchies are strangely separate.
//
// CleanupCallback wraps another Callback and provides the ability to register
// objects for deletion as well as cleanup tasks that will be run on the
// callback's destruction.  The deletion and cleanup tasks will be run on
// whatever thread the CleanupCallback is destroyed in.

#ifndef MEDIA_BASE_CALLBACK_
#define MEDIA_BASE_CALLBACK_

#include <vector>

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

class TaskToCallbackAdapter : public Callback0::Type {
 public:
  static Callback0::Type* NewCallback(Task* task) {
    return new TaskToCallbackAdapter(task);
  }

  virtual ~TaskToCallbackAdapter() {}

  virtual void RunWithParams(const Tuple0& params) { task_->Run(); }

 private:
  TaskToCallbackAdapter(Task* task) : task_(task) {}

  scoped_ptr<Task> task_;

  DISALLOW_COPY_AND_ASSIGN(TaskToCallbackAdapter);
};

template <typename CallbackType>
class CleanupCallback : public CallbackType {
 public:
  explicit CleanupCallback(CallbackType* callback) : callback_(callback) {}

  virtual ~CleanupCallback() {
    for (size_t i = 0; i < run_when_done_.size(); i++) {
      run_when_done_[i]->Run();
      delete run_when_done_[i];
    }
  }

  virtual void RunWithParams(const typename CallbackType::TupleType& params) {
    callback_->RunWithParams(params);
  }

  template <typename T>
  void DeleteWhenDone(T* ptr) {
    RunWhenDone(new DeleteTask<T>(ptr));
  }

  void RunWhenDone(Task* ptr) {
    run_when_done_.push_back(ptr);
  }

 private:
  scoped_ptr<CallbackType> callback_;
  std::vector<Task*> run_when_done_;

  DISALLOW_COPY_AND_ASSIGN(CleanupCallback);
};

}  // namespace media

#endif  // MEDIA_BASE_CALLBACK_
