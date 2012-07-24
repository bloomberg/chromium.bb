/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_PTHREAD_PTHREAD_H_
#define LIBRARIES_PTHREAD_PTHREAD_H_

/**
* Implementation of pthread.h for building the SDK natively on Windows.
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef int pthread_mutex_t;

int pthread_mutex_init(pthread_mutex_t* m, void* traits);
int pthread_mutex_destroy(pthread_mutex_t* m);

int pthread_mutex_lock(pthread_mutex_t* m);
int pthread_mutex_unlock(pthread_mutex_t* m);

#ifdef __cplusplus
}
#endif

#endif  /* LIBRARIES_PTHREAD_PTHREAD_H_ */