// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ============================================================================
// ****************************************************************************
// *  THIS HEADER IS DEPRECATED, SEE base/callback.h FOR NEW IMPLEMENTATION   *
// ****************************************************************************
// ============================================================================
// ============================================================================
// ****************************************************************************
// *  THIS HEADER IS DEPRECATED, SEE base/callback.h FOR NEW IMPLEMENTATION   *
// ****************************************************************************
// ============================================================================
// ============================================================================
// ****************************************************************************
// *  THIS HEADER IS DEPRECATED, SEE base/callback.h FOR NEW IMPLEMENTATION   *
// ****************************************************************************
// ============================================================================
// ============================================================================
// ****************************************************************************
// *  THIS HEADER IS DEPRECATED, SEE base/callback.h FOR NEW IMPLEMENTATION   *
// ****************************************************************************
// ============================================================================
// ============================================================================
// ****************************************************************************
// *  THIS HEADER IS DEPRECATED, SEE base/callback.h FOR NEW IMPLEMENTATION   *
// ****************************************************************************
// ============================================================================
#ifndef BASE_TASK_H_
#define BASE_TASK_H_
#pragma once

#include "base/base_export.h"
#include "base/callback.h"
#include "base/debug/alias.h"
#include "base/memory/raw_scoped_refptr_mismatch_checker.h"
#include "base/memory/weak_ptr.h"
#include "base/tuple.h"

namespace base {
const size_t kDeadTask = 0xDEAD7A53;
}

// Task ------------------------------------------------------------------------
//
// A task is a generic runnable thingy, usually used for running code on a
// different thread or for scheduling future tasks off of the message loop.

class BASE_EXPORT Task {
 public:
  Task();
  virtual ~Task();

  // Tasks are automatically deleted after Run is called.
  virtual void Run() = 0;
};

template<typename T>
void DeletePointer(T* obj) {
  delete obj;
}

namespace base {

// ScopedClosureRunner is akin to scoped_ptr for Closures. It ensures that the
// Closure is executed and deleted no matter how the current scope exits.
class BASE_EXPORT ScopedClosureRunner {
 public:
  explicit ScopedClosureRunner(const Closure& closure);
  ~ScopedClosureRunner();

  Closure Release();

 private:
  Closure closure_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedClosureRunner);
};

namespace subtle {

// This class is meant for use in the implementation of MessageLoop classes
// such as MessageLoop, MessageLoopProxy, BrowserThread, and WorkerPool to
// implement the compatibility APIs while we are transitioning from Task to
// Callback.
//
// It should NOT be used anywhere else!
//
// In particular, notice that this is RefCounted instead of
// RefCountedThreadSafe.  We rely on the fact that users of this class are
// careful to ensure that a lock is taken during transfer of ownership for
// objects from this class to ensure the refcount is not corrupted.
class TaskClosureAdapter : public RefCounted<TaskClosureAdapter> {
 public:
  explicit TaskClosureAdapter(Task* task);

  // |should_leak_task| points to a flag variable that can be used to determine
  // if this class should leak the Task on destruction.  This is important
  // at MessageLoop shutdown since not all tasks can be safely deleted without
  // running.  See MessageLoop::DeletePendingTasks() for the exact behavior
  // of when a Task should be deleted. It is subtle.
  TaskClosureAdapter(Task* task, bool* should_leak_task);

  void Run();

 private:
  friend class base::RefCounted<TaskClosureAdapter>;

  ~TaskClosureAdapter();

  Task* task_;
  bool* should_leak_task_;
  static bool kTaskLeakingDefault;
};

}  // namespace subtle

}  // namespace base

#endif  // BASE_TASK_H_
