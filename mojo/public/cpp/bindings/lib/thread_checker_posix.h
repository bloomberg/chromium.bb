// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_CHECKER_POSIX_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_CHECKER_POSIX_H_

#include <pthread.h>

#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace internal {

// An implementation of ThreadChecker for POSIX systems.
class ThreadCheckerPosix {
 public:
  ThreadCheckerPosix();
  ~ThreadCheckerPosix();

  bool CalledOnValidThread() const MOJO_WARN_UNUSED_RESULT;
 private:
  const pthread_t attached_thread_id_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ThreadCheckerPosix);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_CHECKER_POSIX_H_
