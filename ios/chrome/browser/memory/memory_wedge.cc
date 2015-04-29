// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/memory/memory_wedge.h"

#include <stdlib.h>

#include "base/logging.h"

namespace {

// Reference to the memory wedge. It is useful to:
// - make sure the compiler optimizations in Release don't skip |memset| and
//   really end up putting the wedge in resident memory;
// - free it in tests.
static void* gMemoryWedge;

// The number of bytes in a megabyte.
const size_t kNumBytesInMB = 1024 * 1024;

}  // namespace

namespace memory_wedge {

void AddWedge(size_t wedge_size_in_mb) {
  DCHECK(!gMemoryWedge);
  if (wedge_size_in_mb == 0)
    return;

  // Allocate a wedge and write to it to have it in resident memory.
  const size_t wedge_size = wedge_size_in_mb * kNumBytesInMB;
  gMemoryWedge = malloc(wedge_size);
  memset(gMemoryWedge, -1, wedge_size);
}

void RemoveWedgeForTesting() {
  DCHECK(gMemoryWedge);
  free(gMemoryWedge);
  gMemoryWedge = nullptr;
}

}  // namespace memory_wedge
