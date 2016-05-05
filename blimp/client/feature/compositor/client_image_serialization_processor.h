// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_FEATURE_COMPOSITOR_CLIENT_IMAGE_SERIALIZATION_PROCESSOR_H_
#define BLIMP_CLIENT_FEATURE_COMPOSITOR_CLIENT_IMAGE_SERIALIZATION_PROCESSOR_H_

#include "base/macros.h"
#include "cc/proto/image_serialization_processor.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkPixelSerializer;

namespace blimp {
namespace client {

// ClientImageSerializationProcessor provides functionality to deserialize Skia
// images.
class ClientImageSerializationProcessor
    : public cc::ImageSerializationProcessor {
 public:
  ClientImageSerializationProcessor();
  ~ClientImageSerializationProcessor();

  // cc::ImageSerializationProcessor implementation.
  SkPixelSerializer* GetPixelSerializer() override;
  SkPicture::InstallPixelRefProc GetPixelDeserializer() override;

 private:
  SkPicture::InstallPixelRefProc pixel_deserializer_;

  DISALLOW_COPY_AND_ASSIGN(ClientImageSerializationProcessor);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_FEATURE_COMPOSITOR_CLIENT_IMAGE_SERIALIZATION_PROCESSOR_H_
