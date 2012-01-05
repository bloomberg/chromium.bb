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

}  // namespace base

#endif  // BASE_TASK_H_
