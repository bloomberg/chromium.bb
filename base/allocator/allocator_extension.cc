// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_extension.h"

#include "base/logging.h"

#if defined(USE_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#include "third_party/tcmalloc/chromium/src/gperftools/malloc_extension.h"
#endif

namespace base {
namespace allocator {

void ReleaseFreeMemory() {
#if defined(USE_TCMALLOC)
  ::MallocExtension::instance()->ReleaseFreeMemory();
#endif
}

bool GetNumericProperty(const char* name, size_t* value) {
#if defined(USE_TCMALLOC)
  return ::MallocExtension::instance()->GetNumericProperty(name, value);
#endif
  return false;
}

bool IsHeapProfilerRunning() {
#if defined(USE_TCMALLOC)
  return ::IsHeapProfilerRunning();
#endif
  return false;
}

}  // namespace allocator
}  // namespace base
