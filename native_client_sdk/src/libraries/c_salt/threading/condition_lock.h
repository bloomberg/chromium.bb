// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONDITION_LOCK_H_
#define CONDITION_LOCK_H_

#include <pthread.h>

namespace threading {
// Class to manage a lock associated with a specific value.  The calling thread
// can ask to acquire the lock only when the lock is in a certain condition.
class ConditionLock {
 public:
  ConditionLock() : condition_value_(0) {
    InitLock();
  }
  explicit ConditionLock(int32_t condition_value)
      : condition_value_(condition_value) {
    InitLock();
  }

  virtual ~ConditionLock() {
    pthread_cond_destroy(&condition_condition_);
    pthread_mutex_destroy(&condition_lock_);
  }

  // Acquire the mutex without regard to the condition.
  void Lock() {
    pthread_mutex_lock(&condition_lock_);
  }

  // Acquire the mutex lock when the lock values are equal.  Blocks the
  // calling thread until the lock can be acquired and the condition is met.
  void LockWhenCondition(int32_t condition_value) {
    Lock();
    while (condition_value != condition_value_) {
      pthread_cond_wait(&condition_condition_, &condition_lock_);
    }
    // When this method returns, |contition_lock_| will be acquired.  The
    // calling thread must unlock it.
  }

  // Acquire the mutex lock when the lock values are _NOT_ equal.  Blocks the
  // calling thread until the lock can be acquired and the condition is met.
  void LockWhenNotCondition(int32_t condition_value) {
    Lock();
    while (condition_value == condition_value_) {
      pthread_cond_wait(&condition_condition_, &condition_lock_);
    }
    // When this method returns, |contition_lock_| will be acquired.  The
    // calling thread must unlock it.
  }

  // Release the lock without changing the condition.  Signal the condition
  // so that threads waiting in LockWhenCondtion() will wake up.  If there are
  // no threads waiting for the signal, this has the same effect as a simple
  // mutex unlock.
  void Unlock() {
    pthread_cond_broadcast(&condition_condition_);
    pthread_mutex_unlock(&condition_lock_);
  }

  // Release the lock, setting the condition's value.  This assumes that
  // |condition_lock_| has been acquired.
  void UnlockWithCondition(unsigned int condition_value) {
    condition_value_ = condition_value;
    Unlock();
  }

  // Return the current condition value without any mutex protection.
  int32_t condition_value() const {
    return condition_value_;
  }

 private:
  void InitLock() {
    pthread_mutex_init(&condition_lock_, NULL);
    pthread_cond_init(&condition_condition_, NULL);
  }

  pthread_mutex_t condition_lock_;
  pthread_cond_t condition_condition_;
  int32_t condition_value_;
};
}  // namespace threading

#endif  // CONDITION_LOCK_H_

