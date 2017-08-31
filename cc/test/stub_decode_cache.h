// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_STUB_DECODE_CACHE_H_
#define CC_TEST_STUB_DECODE_CACHE_H_

#include "cc/tiles/image_decode_cache.h"

namespace cc {

class StubDecodeCache : public ImageDecodeCache {
 public:
  StubDecodeCache() = default;
  ~StubDecodeCache() override = default;

  bool GetTaskForImageAndRef(const DrawImage& image,
                             const TracingInfo& tracing_info,
                             scoped_refptr<TileTask>* task) override;
  bool GetOutOfRasterDecodeTaskForImageAndRef(
      const DrawImage& image,
      scoped_refptr<TileTask>* task) override;
  void UnrefImage(const DrawImage& image) override {}
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override {}
  void ReduceCacheUsage() override {}
  void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) override {}
  void ClearCache() override {}
  size_t GetMaximumMemoryLimitBytes() const override;
  void NotifyImageUnused(const PaintImage::FrameKey& frame_key) override {}
};

}  // namespace cc

#endif  // CC_TEST_STUB_DECODE_CACHE_H_
