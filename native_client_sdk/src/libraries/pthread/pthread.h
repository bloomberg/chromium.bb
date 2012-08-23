/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_PTHREAD_PTHREAD_H_
#define LIBRARIES_PTHREAD_PTHREAD_H_

/**
* Implementation of pthread.h for building the SDK natively on Windows.
*/

#include <windows.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef HANDLE pthread_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;

int pthread_mutex_init(pthread_mutex_t* m, void* attrs);
int pthread_mutex_destroy(pthread_mutex_t* m);
int pthread_mutex_lock(pthread_mutex_t* m);
int pthread_mutex_unlock(pthread_mutex_t* m);
int pthread_mutex_trylock(pthread_mutex_t* m);

int pthread_cond_init(pthread_cond_t* c, void* attrs);
int pthread_cond_destroy(pthread_cond_t* c);
int pthread_cond_broadcast(pthread_cond_t* c);
int pthread_cond_signal(pthread_cond_t* c);
int pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m);

int pthread_create(pthread_t* t,
                   void* attrs,
                   void* (*start_routine)(void*),
                   void* arg);

#ifdef __cplusplus
}
#endif

#endif  /* LIBRARIES_PTHREAD_PTHREAD_H_ */
