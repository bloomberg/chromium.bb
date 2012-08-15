// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef GTEST_PPAPI_REF_COUNT_H_
#define GTEST_PPAPI_REF_COUNT_H_

#include "gtest_ppapi/pthread_ext.h"

// A thread-safe reference counter for class CompletionCallbackFactory.
class RefCount {
 public:
  RefCount() : ref_(0) {
    pthread_mutex_init(&mutex_, NULL);
  }
  ~RefCount() {
    pthread_mutex_destroy(&mutex_);
  }

  int32_t AddRef() {
    int32_t ret_val = 0;
    if (pthread_mutex_lock(&mutex_) == PTHREAD_MUTEX_SUCCESS) {
      ret_val = ++ref_;
      pthread_mutex_unlock(&mutex_);
    }
    return ret_val;
  }

  int32_t Release() {
    int32_t ret_val = -1;
    if (pthread_mutex_lock(&mutex_) == PTHREAD_MUTEX_SUCCESS) {
      ret_val = --ref_;
      pthread_mutex_unlock(&mutex_);
    }
    return ret_val;
  }

 private:
  int32_t ref_;
  pthread_mutex_t mutex_;
};

#endif  // GTEST_PPAPI_REF_COUNT_H_
