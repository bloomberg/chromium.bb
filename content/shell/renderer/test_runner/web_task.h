// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TASK_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TASK_H_

#include <vector>

#include "base/macros.h"

namespace content {

class WebTaskList;

// WebTask represents a task which can run by WebTestDelegate::postTask() or
// WebTestDelegate::postDelayedTask().
class WebTask {
 public:
  explicit WebTask(WebTaskList*);
  virtual ~WebTask();

  // The main code of this task.
  // An implementation of run() should return immediately if cancel() was
  // called.
  virtual void run() = 0;
  virtual void cancel() = 0;

 protected:
  WebTaskList* task_list_;
};

class WebTaskList {
 public:
  WebTaskList();
  ~WebTaskList();
  void RegisterTask(WebTask*);
  void UnregisterTask(WebTask*);
  void RevokeAll();

 private:
  std::vector<WebTask*> tasks_;

  DISALLOW_COPY_AND_ASSIGN(WebTaskList);
};

// A task containing an object pointer of class T. Derived classes should
// override RunIfValid() which in turn can safely invoke methods on the
// object_. The Class T must have "WebTaskList* mutable_task_list()".
template <class T>
class WebMethodTask : public WebTask {
 public:
  explicit WebMethodTask(T* object)
      : WebTask(object->mutable_task_list()), object_(object) {}

  virtual ~WebMethodTask() {}

  virtual void run() {
    if (object_)
      RunIfValid();
  }

  virtual void cancel() {
    object_ = 0;
    task_list_->UnregisterTask(this);
    task_list_ = 0;
  }

  virtual void RunIfValid() = 0;

 protected:
  T* object_;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TASK_H_
