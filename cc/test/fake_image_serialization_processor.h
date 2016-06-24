// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_IMAGE_SERIALIZATION_PROCESSOR_H_
#define CC_TEST_FAKE_IMAGE_SERIALIZATION_PROCESSOR_H_

#include <memory>

#include "base/macros.h"
#include "cc/blimp/client_picture_cache.h"
#include "cc/blimp/engine_picture_cache.h"
#include "cc/blimp/image_serialization_processor.h"
#include "cc/test/picture_cache_model.h"

namespace cc {

class FakeImageSerializationProcessor : public ImageSerializationProcessor {
 public:
  FakeImageSerializationProcessor();
  ~FakeImageSerializationProcessor() override;

  // ImageSerializationProcessor implementation.
  std::unique_ptr<EnginePictureCache> CreateEnginePictureCache() override;
  std::unique_ptr<ClientPictureCache> CreateClientPictureCache() override;

 private:
  // A PictureCacheModel shared between both the engine and client cache.
  std::unique_ptr<PictureCacheModel> picture_cache_model_;

  DISALLOW_COPY_AND_ASSIGN(FakeImageSerializationProcessor);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_IMAGE_SERIALIZATION_PROCESSOR_H_
