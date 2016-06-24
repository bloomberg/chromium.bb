// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_image_serialization_processor.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/test/fake_client_picture_cache.h"
#include "cc/test/fake_engine_picture_cache.h"
#include "cc/test/picture_cache_model.h"

namespace cc {

FakeImageSerializationProcessor::FakeImageSerializationProcessor()
    : picture_cache_model_(base::WrapUnique(new PictureCacheModel)) {}

FakeImageSerializationProcessor::~FakeImageSerializationProcessor() {}

std::unique_ptr<EnginePictureCache>
FakeImageSerializationProcessor::CreateEnginePictureCache() {
  return base::WrapUnique(
      new FakeEnginePictureCache(picture_cache_model_.get()));
}

std::unique_ptr<ClientPictureCache>
FakeImageSerializationProcessor::CreateClientPictureCache() {
  return base::WrapUnique(
      new FakeClientPictureCache(picture_cache_model_.get()));
}

}  // namespace cc
