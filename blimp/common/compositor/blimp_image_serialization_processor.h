// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_COMPOSITOR_BLIMP_IMAGE_SERIALIZATION_PROCESSOR_H_
#define BLIMP_COMMON_COMPOSITOR_BLIMP_IMAGE_SERIALIZATION_PROCESSOR_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/common/blimp_common_export.h"
#include "cc/proto/image_serialization_processor.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkPixelSerializer;

namespace blimp {

// BlimpImageSerializationProcessor provides functionality to serialize and
// deserialize Skia images.
class BLIMP_COMMON_EXPORT BlimpImageSerializationProcessor
    : public cc::ImageSerializationProcessor {
 public:
  // Denotes whether the BlimpImageSerializationProcessor should act as a
  // serializer (engine) or deserializer (client).
  enum class Mode { SERIALIZATION, DESERIALIZATION };
  explicit BlimpImageSerializationProcessor(Mode mode);
  ~BlimpImageSerializationProcessor();

  // cc::ImageSerializationProcessor implementation.
  SkPixelSerializer* GetPixelSerializer() override;
  SkPicture::InstallPixelRefProc GetPixelDeserializer() override;

 private:
  scoped_ptr<SkPixelSerializer> pixel_serializer_;
  SkPicture::InstallPixelRefProc pixel_deserializer_;

  DISALLOW_COPY_AND_ASSIGN(BlimpImageSerializationProcessor);
};

}  // namespace blimp

#endif  // BLIMP_COMMON_COMPOSITOR_BLIMP_IMAGE_SERIALIZATION_PROCESSOR_H_
