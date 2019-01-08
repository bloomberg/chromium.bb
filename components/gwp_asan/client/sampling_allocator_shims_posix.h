// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_ALLOCATOR_SHIMS_POSIX_H_
#define COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_ALLOCATOR_SHIMS_POSIX_H_

#include <pthread.h>
#include <stddef.h>

#include "base/logging.h"

namespace gwp_asan {
namespace internal {

// Due to https://crbug.com/881352 the sampling allocator shims can not use
// base::ThreadLocalStorage to access the TLS. Instead, we use platform-specific
// TLS APIs.
//
// TODO(vtsyrklevich): This implementation is identical to code in the
// base::PoissonAllocationSampler, see if they can share code.
using TLSKey = pthread_key_t;

void TLSInit(TLSKey* key) {
  int result = pthread_key_create(key, nullptr);
  CHECK_EQ(0, result);
}

size_t TLSGetValue(const TLSKey& key) {
  return reinterpret_cast<size_t>(pthread_getspecific(key));
}

void TLSSetValue(const TLSKey& key, size_t value) {
  pthread_setspecific(key, reinterpret_cast<void*>(value));
}

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_ALLOCATOR_SHIMS_POSIX_H_
