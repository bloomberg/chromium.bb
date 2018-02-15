// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BIND_HELPERS_H_
#define BASE_BIND_HELPERS_H_

#include <stddef.h>

#include <type_traits>
#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"

// This defines a set of simple functions and utilities that people want when
// using Callback<> and Bind().

namespace base {

// Useful for creating a Closure that does nothing when called.
BASE_EXPORT void DoNothing();

// Useful for creating a Closure that will delete a pointer when invoked. Only
// use this when necessary. In most cases MessageLoop::DeleteSoon() is a better
// fit.
template<typename T>
void DeletePointer(T* obj) {
  delete obj;
}

}  // namespace base

#endif  // BASE_BIND_HELPERS_H_
