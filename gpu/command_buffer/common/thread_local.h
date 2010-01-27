// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions for allocating and accessing thread local values via key.

#ifndef GPU_COMMAND_BUFFER_COMMON_THREAD_LOCAL_H_
#define GPU_COMMAND_BUFFER_COMMON_THREAD_LOCAL_H_

#include <build/build_config.h>

#if defined(OS_WIN)
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace gpu {

#if defined(OS_WIN)
typedef DWORD ThreadLocalKey;
#else
typedef pthread_key_t ThreadLocalKey;
#endif

inline ThreadLocalKey ThreadLocalAlloc() {
#if defined(OS_WIN)
  return TlsAlloc();
#else
  ThreadLocalKey key;
  pthread_key_create(&key, NULL);
  return key;
#endif
}

inline void ThreadLocalFree(ThreadLocalKey key) {
#if defined(OS_WIN)
  TlsFree(key);
#else
  pthread_key_delete(key);
#endif
}

inline void ThreadLocalSetValue(ThreadLocalKey key, void* value) {
#if defined(OS_WIN)
  TlsSetValue(key, value);
#else
  pthread_setspecific(key, value);
#endif
}

inline void* ThreadLocalGetValue(ThreadLocalKey key) {
#if defined(OS_WIN)
  return TlsGetValue(key);
#else
  return pthread_getspecific(key);
#endif
}
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_THREAD_LOCAL_H_
