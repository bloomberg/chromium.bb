// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CancelInDestructor : public base::RefCounted<CancelInDestructor> {
 public:
  CancelInDestructor() : cancelable_task_(NULL) {}

  void Start() {
    if (cancelable_task_) {
      ADD_FAILURE();
      return;
    }
    AddRef();
    cancelable_task_ = NewRunnableMethod(
        this, &CancelInDestructor::NeverIssuedCallback);
    Release();
  }

  CancelableTask* cancelable_task() {
    return cancelable_task_;
  }

 private:
  friend class base::RefCounted<CancelInDestructor>;

  ~CancelInDestructor() {
    if (cancelable_task_)
      cancelable_task_->Cancel();
  }

  void NeverIssuedCallback() { NOTREACHED(); }

  CancelableTask* cancelable_task_;
};

TEST(TaskTest, TestCancelInDestructor) {
  // Intentionally not using a scoped_refptr for cancel_in_destructor.
  CancelInDestructor* cancel_in_destructor = new CancelInDestructor();
  cancel_in_destructor->Start();
  CancelableTask* cancelable_task = cancel_in_destructor->cancelable_task();
  ASSERT_TRUE(cancelable_task);
  delete cancelable_task;
}

}  // namespace
