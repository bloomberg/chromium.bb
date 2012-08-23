/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <windows.h>
#include <errno.h>
#include "pthread.h"

int pthread_mutex_init(pthread_mutex_t* m, void* attrs) {
  InitializeCriticalSection(m);
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* m) {
  DeleteCriticalSection(m);
  return 0;
}

int pthread_mutex_lock(pthread_mutex_t* m) {
  EnterCriticalSection(m);
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* m) {
  LeaveCriticalSection(m);
  return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* m) {
  if (TryEnterCriticalSection(m))
    return 0;
  return EBUSY;
}

int pthread_cond_init(pthread_cond_t* c, void* attrs) {
  InitializeConditionVariable(c);
  return 0;
}

int pthread_cond_destroy(pthread_cond_t* c) {
  return 0;
}

int pthread_cond_broadcast(pthread_cond_t* c) {
  WakeAllConditionVariable(c);
  return 0;
}

int pthread_cond_signal(pthread_cond_t* c) {
  WakeConditionVariable(c);
  return 0;
}

int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m) {
  if (SleepConditionVariableCS(c, m, INFINITE))
    return 0;
  return EINVAL;
}

typedef struct {
  void* (*start_routine)(void*);
  void* arg;
} PthreadCreateInfo;

static DWORD WINAPI PthreadCreateThreadFunc(LPVOID param) {
  PthreadCreateInfo* pthread_create_info = (PthreadCreateInfo*)param;
  void* result = (pthread_create_info->start_routine)(pthread_create_info->arg);
  (void)result;  // Ignore result
  HeapFree(GetProcessHeap(), 0, pthread_create_info);
  return 0;
}

int pthread_create(pthread_t* t,
                   void* attrs,
                   void* (*start_routine)(void*),
                   void* arg) {
  HANDLE thread_handle;
  PthreadCreateInfo* pthread_create_info;
 
  pthread_create_info = (PthreadCreateInfo*)HeapAlloc(
      GetProcessHeap(),
      0,
      sizeof(PthreadCreateInfo));
  if (pthread_create_info == NULL)
    return EAGAIN;

  pthread_create_info->start_routine = start_routine;
  pthread_create_info->arg = arg;

  thread_handle = CreateThread(NULL,  // lpThreadAttributes
                               0,  // dwStackSize
                               &PthreadCreateThreadFunc,  // lpStartAddress
                               pthread_create_info,  // lpParameter,
                               0,  // dwCreationFlags
                               NULL);  // lpThreadId
  if (thread_handle == NULL) {
    HeapFree(GetProcessHeap(), 0, pthread_create_info);
    return EAGAIN;
  }

  *t = thread_handle;
  return 0;
}
