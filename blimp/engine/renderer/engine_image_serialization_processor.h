// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_RENDERER_ENGINE_IMAGE_SERIALIZATION_PROCESSOR_H_
#define BLIMP_ENGINE_RENDERER_ENGINE_IMAGE_SERIALIZATION_PROCESSOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "blimp/common/blimp_common_export.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/engine/mojo/blob_channel.mojom.h"
#include "cc/proto/image_serialization_processor.h"
#include "third_party/libwebp/webp/encode.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelSerializer.h"

namespace content {
class RenderFrame;
}  // class content

namespace blimp {
namespace engine {

class BlobChannelSenderProxy;

// EngineImageSerializationProcessor provides functionality to serialize and
// deserialize Skia images.
class BLIMP_COMMON_EXPORT EngineImageSerializationProcessor
    : public cc::ImageSerializationProcessor,
      public SkPixelSerializer {
 public:
  explicit EngineImageSerializationProcessor(
      std::unique_ptr<BlobChannelSenderProxy> blob_channel);
  ~EngineImageSerializationProcessor() override;

  // cc::ImageSerializationProcessor implementation.
  SkPixelSerializer* GetPixelSerializer() override;
  SkPicture::InstallPixelRefProc GetPixelDeserializer() override;

  // SkPixelSerializer implementation.
  bool onUseEncodedData(const void* data, size_t len) override;
  SkData* onEncode(const SkPixmap& pixmap) override;

 private:
  scoped_refptr<BlobData> EncodeImageAsBlob(const SkPixmap& pixmap);

  std::unique_ptr<SkPixelSerializer> pixel_serializer_;

  // Sends image data as blobs to the browser process.
  std::unique_ptr<BlobChannelSenderProxy> blob_channel_;

  // Reusable output buffer for image encoding.
  WebPMemoryWriter writer_state_;

  DISALLOW_COPY_AND_ASSIGN(EngineImageSerializationProcessor);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_RENDERER_ENGINE_IMAGE_SERIALIZATION_PROCESSOR_H_
