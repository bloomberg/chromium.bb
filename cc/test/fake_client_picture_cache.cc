// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_client_picture_cache.h"

#include "cc/test/picture_cache_model.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {

FakeClientPictureCache::FakeClientPictureCache(PictureCacheModel* model)
    : model_(model) {}

FakeClientPictureCache::~FakeClientPictureCache() = default;

const std::vector<uint32_t>& FakeClientPictureCache::GetAllUsedPictureIds() {
  return used_picture_ids_;
}

void FakeClientPictureCache::MarkUsed(uint32_t engine_picture_id) {
  used_picture_ids_.push_back(engine_picture_id);
}

sk_sp<const SkPicture> FakeClientPictureCache::GetPicture(uint32_t unique_id) {
  if (!model_)
    return nullptr;

  return model_->GetPicture(unique_id);
}

void FakeClientPictureCache::ApplyCacheUpdate(
    const std::vector<PictureData>& cache_update) {}

void FakeClientPictureCache::Flush() {}

}  // namespace cc
