// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ALL_RENDERING_BENCHMARKS_H_
#define CONTENT_RENDERER_ALL_RENDERING_BENCHMARKS_H_

#include <vector>

#include "base/memory/scoped_vector.h"

namespace content {
class RenderingBenchmark;

ScopedVector<RenderingBenchmark> AllRenderingBenchmarks();

}  // namespace content

#endif  // CONTENT_RENDERER_ALL_RENDERING_BENCHMARKS_H_
