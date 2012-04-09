/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <windows.h>
#include <errno.h>
#include "pthread.h"

int pthread_mutex_init(pthread_mutex_t* m, void* traits) {
  *m = (int) CreateMutex(NULL, 0, NULL);
  return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* m) {
  CloseHandle((HANDLE) *m);
  return 0;
}

int pthread_mutex_lock(pthread_mutex_t* m) {
  if (WaitForSingleObject((HANDLE) *m, INFINITE) == WAIT_OBJECT_0)
    return 0;

  return EINVAL;
}

int pthread_mutex_unlock(pthread_mutex_t* m) {
  if (ReleaseMutex((HANDLE) *m)) return 0;

  return EINVAL;
}

int pthread_mutex_trylock(pthread_mutex_t* m) {
  int val = WaitForSingleObject((HANDLE) *m, 0);
  
  if (val == WAIT_OBJECT_0) return 0;

  if (val == WAIT_TIMEOUT) {
    errno = EBUSY;
  }
  else {
    errno = EINVAL;
  }
  return -1;
}