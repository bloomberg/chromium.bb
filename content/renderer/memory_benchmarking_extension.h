// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEMORY_BENCHMARKING_EXTENSION_H_
#define CONTENT_RENDERER_MEMORY_BENCHMARKING_EXTENSION_H_

#include "v8/include/v8.h"

namespace content {

class MemoryBenchmarkingExtension {
 public:
  static v8::Extension* Get();
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEMORY_BENCHMARKING_EXTENSION_H_
