// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_IMAGE_SERIALIZATION_PROCESSOR_H_
#define CC_TEST_FAKE_IMAGE_SERIALIZATION_PROCESSOR_H_

#include "base/macros.h"
#include "cc/proto/image_serialization_processor.h"

class SkPixelSerializer;

namespace cc {

class FakeImageSerializationProcessor : public ImageSerializationProcessor {
 public:
  FakeImageSerializationProcessor();
  ~FakeImageSerializationProcessor();

  // ImageSerializationProcessor implementation.
  SkPixelSerializer* GetPixelSerializer() override;
  SkPicture::InstallPixelRefProc GetPixelDeserializer() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeImageSerializationProcessor);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_IMAGE_SERIALIZATION_PROCESSOR_H_
