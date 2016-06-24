// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CLIENT_PICTURE_CACHE_H_
#define CC_TEST_FAKE_CLIENT_PICTURE_CACHE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "cc/blimp/client_picture_cache.h"

namespace cc {
class PictureCacheModel;

class FakeClientPictureCache : public ClientPictureCache {
 public:
  explicit FakeClientPictureCache(PictureCacheModel* model);
  ~FakeClientPictureCache() override;

  const std::vector<uint32_t>& GetAllUsedPictureIds();

  // ClientPictureCache implementation.
  void MarkUsed(uint32_t engine_picture_id) override;
  sk_sp<const SkPicture> GetPicture(uint32_t unique_id) override;
  void ApplyCacheUpdate(const std::vector<PictureData>& cache_update) override;
  void Flush() override;

 private:
  PictureCacheModel* model_;
  std::vector<uint32_t> used_picture_ids_;

  DISALLOW_COPY_AND_ASSIGN(FakeClientPictureCache);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CLIENT_PICTURE_CACHE_H_
