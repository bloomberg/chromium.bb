// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/stub_decode_cache.h"

namespace cc {

bool StubDecodeCache::GetTaskForImageAndRef(const DrawImage& image,
                                            const TracingInfo& tracing_info,
                                            scoped_refptr<TileTask>* task) {
  return true;
}

bool StubDecodeCache::GetOutOfRasterDecodeTaskForImageAndRef(
    const DrawImage& image,
    scoped_refptr<TileTask>* task) {
  return true;
}

DecodedDrawImage StubDecodeCache::GetDecodedImageForDraw(
    const DrawImage& image) {
  return DecodedDrawImage(nullptr, SkSize::MakeEmpty(),
                          SkSize::Make(1.0f, 1.0f), kNone_SkFilterQuality);
}

size_t StubDecodeCache::GetMaximumMemoryLimitBytes() const {
  return 0u;
}

}  // namespace cc
