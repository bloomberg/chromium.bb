// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SCHEDULER_UNIQUE_STACK_H_
#define BASE_TASK_SCHEDULER_SCHEDULER_UNIQUE_STACK_H_

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"

namespace base {
namespace internal {

// A stack that supports removal of arbitrary values and doesn't allow multiple
// insertions of the same value. This class is NOT thread-safe.
template <typename T>
class SchedulerUniqueStack {
 public:
  SchedulerUniqueStack();
  ~SchedulerUniqueStack();

  // Inserts |val| at the top of the stack. |val| must not already be on the
  // stack.
  void Push(const T& val);

  // Removes the top value from the stack and returns it. Cannot be called on an
  // empty stack.
  T Pop();

  // Removes |val| from the stack.
  void Remove(const T& val);

  // Returns the number of values on the stack.
  size_t Size() const;

  // Returns true if the stack is empty.
  bool Empty() const;

 private:
  std::vector<T> stack_;

  DISALLOW_COPY_AND_ASSIGN(SchedulerUniqueStack);
};

template <typename T>
SchedulerUniqueStack<T>::SchedulerUniqueStack() = default;

template <typename T>
SchedulerUniqueStack<T>::~SchedulerUniqueStack() = default;

template <typename T>
void SchedulerUniqueStack<T>::Push(const T& val) {
  DCHECK(std::find(stack_.begin(), stack_.end(), val) == stack_.end())
      << "Value already on stack";
  stack_.push_back(val);
}

template <typename T>
T SchedulerUniqueStack<T>::Pop() {
  DCHECK(!stack_.empty());
  const T val = stack_.back();
  stack_.pop_back();
  return val;
}

template <typename T>
void SchedulerUniqueStack<T>::Remove(const T& val) {
  auto it = std::find(stack_.begin(), stack_.end(), val);
  if (it != stack_.end())
    stack_.erase(it);
}

template <typename T>
size_t SchedulerUniqueStack<T>::Size() const {
  return stack_.size();
}

template <typename T>
bool SchedulerUniqueStack<T>::Empty() const {
  return stack_.empty();
}

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SCHEDULER_UNIQUE_STACK_H_
