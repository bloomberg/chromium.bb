// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_engine_picture_cache.h"

#include <map>
#include <memory>

#include "cc/blimp/picture_data.h"
#include "cc/playback/display_item_list.h"
#include "cc/test/picture_cache_model.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {

FakeEnginePictureCache::FakeEnginePictureCache(PictureCacheModel* model)
    : model_(model) {}

FakeEnginePictureCache::~FakeEnginePictureCache() {}

void FakeEnginePictureCache::MarkAllSkPicturesAsUsed(
    const DisplayItemList* display_list) {
  if (!display_list)
    return;

  for (auto it = display_list->begin(); it != display_list->end(); ++it) {
    sk_sp<const SkPicture> picture = it->GetPicture();
    if (!picture)
      continue;

    MarkUsed(picture.get());
  }
}

const std::vector<uint32_t>& FakeEnginePictureCache::GetAllUsedPictureIds() {
  return used_picture_ids_;
}

void FakeEnginePictureCache::MarkUsed(const SkPicture* picture) {
  if (model_)
    model_->AddPicture(picture);
  used_picture_ids_.push_back(picture->uniqueID());
}

std::vector<PictureData>
FakeEnginePictureCache::CalculateCacheUpdateAndFlush() {
  return std::vector<PictureData>();
}

}  // namespace cc
