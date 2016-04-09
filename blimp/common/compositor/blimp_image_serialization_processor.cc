// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/compositor/blimp_image_serialization_processor.h"

#include <stddef.h>
#include <vector>

#include "base/logging.h"
#include "blimp/common/compositor/webp_decoder.h"
#include "third_party/libwebp/webp/encode.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelSerializer.h"

namespace blimp {

BlimpImageSerializationProcessor::BlimpImageSerializationProcessor(Mode mode) {
  DCHECK(mode == Mode::DESERIALIZATION);
  pixel_serializer_ = nullptr;
  pixel_deserializer_ = &WebPDecoder;
}

BlimpImageSerializationProcessor::~BlimpImageSerializationProcessor() {}

SkPixelSerializer* BlimpImageSerializationProcessor::GetPixelSerializer() {
  return pixel_serializer_.get();
}

SkPicture::InstallPixelRefProc
BlimpImageSerializationProcessor::GetPixelDeserializer() {
  return pixel_deserializer_;
}

}  // namespace blimp
