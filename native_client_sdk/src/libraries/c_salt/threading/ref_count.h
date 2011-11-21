// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef REF_COUNT_H_
#define REF_COUNT_H_

#include "c_salt/threading/pthread_ext.h"

namespace threading {

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

}  // namespace threading
#endif  // REF_COUNT_H_

