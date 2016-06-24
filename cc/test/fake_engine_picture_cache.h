// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_ENGINE_PICTURE_CACHE_H_
#define CC_TEST_FAKE_ENGINE_PICTURE_CACHE_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "cc/blimp/engine_picture_cache.h"

class SkPicture;

namespace cc {
class DisplayItemList;
class PictureCacheModel;
struct PictureData;

class FakeEnginePictureCache : public EnginePictureCache {
 public:
  explicit FakeEnginePictureCache(PictureCacheModel* model);
  ~FakeEnginePictureCache() override;

  void MarkAllSkPicturesAsUsed(const DisplayItemList* display_list);
  const std::vector<uint32_t>& GetAllUsedPictureIds();

  // EnginePictureCache implementation.
  std::vector<PictureData> CalculateCacheUpdateAndFlush() override;
  void MarkUsed(const SkPicture* picture) override;

 private:
  PictureCacheModel* model_;
  std::vector<uint32_t> used_picture_ids_;

  DISALLOW_COPY_AND_ASSIGN(FakeEnginePictureCache);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_ENGINE_PICTURE_CACHE_H_
