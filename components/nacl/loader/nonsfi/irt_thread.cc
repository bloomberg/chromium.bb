// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/nacl/loader/nonsfi/irt_interfaces.h"

namespace nacl {
namespace nonsfi {
namespace {

// We heuristically chose 1M for the stack size per thread.
const int kStackSize = 1024 * 1024;

// For RAII of pthread_attr_t.
class ScopedPthreadAttrPtr {
 public:
  ScopedPthreadAttrPtr(pthread_attr_t* attr) : attr_(attr) {
  }
  ~ScopedPthreadAttrPtr() {
    pthread_attr_destroy(attr_);
  }

 private:
  pthread_attr_t* attr_;
};

struct ThreadContext {
  void (*start_func)();
  void* thread_ptr;
};

// A thread local pointer to support nacl_irt_tls.
// This should be initialized at the beginning of ThreadMain, which is a thin
// wrapper of a user function called on a newly created thread, and may be
// reset via IrtTlsInit(). The pointer can be obtained via IrtTlsGet().
__thread void* g_thread_ptr;

void* ThreadMain(void *arg) {
  ::scoped_ptr<ThreadContext> context(static_cast<ThreadContext*>(arg));
  g_thread_ptr = context->thread_ptr;

  // Release the memory of context before running start_func.
  void (*start_func)() = context->start_func;
  context.reset();

  start_func();
  abort();
}

int IrtThreadCreate(void (*start_func)(), void* stack, void* thread_ptr) {
  pthread_attr_t attr;
  int error = pthread_attr_init(&attr);
  if (error != 0)
    return error;
  ScopedPthreadAttrPtr scoped_attr_ptr(&attr);

  // Note: Currently we ignore the argument stack.
  error = pthread_attr_setstacksize(&attr, kStackSize);
  if (error != 0)
    return error;

  error = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (error != 0)
    return error;

  ::scoped_ptr<ThreadContext> context(new ThreadContext);
  context->start_func = start_func;
  context->thread_ptr = thread_ptr;

  pthread_t tid;
  error = pthread_create(&tid, &attr, ThreadMain, context.get());
  if (error != 0)
    return error;

  // The ownership of the context is taken by the created thread. So, here we
  // just manually release it.
  ignore_result(context.release());
  return 0;
}

void IrtThreadExit(int32_t* stack_flag) {
  // As we actually don't use stack given to thread_create, it means that the
  // memory can be released whenever.
  if (stack_flag)
    *stack_flag = 0;
  pthread_exit(NULL);
}

int IrtThreadNice(const int nice) {
  // TODO(https://code.google.com/p/nativeclient/issues/detail?id=3734):
  // Implement this method.
  // Note that this is just a hint, so here we just return success without
  // do anything.
  return 0;
}

int IrtTlsInit(void* thread_ptr) {
  g_thread_ptr = thread_ptr;
  return 0;
}

void* IrtTlsGet() {
  return g_thread_ptr;
}

}  // namespace

const nacl_irt_thread kIrtThread = {
  IrtThreadCreate,
  IrtThreadExit,
  IrtThreadNice,
};

const nacl_irt_tls kIrtTls = {
  IrtTlsInit,
  IrtTlsGet,
};

}  // namespace nonsfi
}  // namespace nacl
