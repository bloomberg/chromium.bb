// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_RENDERER_ENGINE_IMAGE_SERIALIZATION_PROCESSOR_H_
#define BLIMP_ENGINE_RENDERER_ENGINE_IMAGE_SERIALIZATION_PROCESSOR_H_

#include <memory>

#include "base/macros.h"
#include "blimp/common/blimp_common_export.h"
#include "blimp/engine/mojo/blob_channel.mojom.h"
#include "cc/proto/image_serialization_processor.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkPixelSerializer;

namespace content {
class RenderFrame;
}  // class content

namespace blimp {
namespace engine {

// EngineImageSerializationProcessor provides functionality to serialize and
// deserialize Skia images.
class BLIMP_COMMON_EXPORT EngineImageSerializationProcessor
    : public cc::ImageSerializationProcessor {
 public:
  explicit EngineImageSerializationProcessor(
      mojom::BlobChannelPtr blob_channel);
  ~EngineImageSerializationProcessor();

  // cc::ImageSerializationProcessor implementation.
  SkPixelSerializer* GetPixelSerializer() override;
  SkPicture::InstallPixelRefProc GetPixelDeserializer() override;

 private:
  std::unique_ptr<SkPixelSerializer> pixel_serializer_;
  mojom::BlobChannelPtr blob_channel_;

  DISALLOW_COPY_AND_ASSIGN(EngineImageSerializationProcessor);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_RENDERER_ENGINE_IMAGE_SERIALIZATION_PROCESSOR_H_
