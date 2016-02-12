// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/compositor/blimp_image_serialization_processor.h"

#include <stddef.h>
#include <vector>

#include "base/logging.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelSerializer.h"

namespace {
bool NoopDecoder(const void* input, size_t input_size, SkBitmap* bitmap) {
  // TODO(nyquist): Add an image decoder.
  return false;
}

}  // namespace

namespace blimp {

BlimpImageSerializationProcessor::BlimpImageSerializationProcessor(Mode mode) {
  switch (mode) {
    case Mode::SERIALIZATION:
      pixel_serializer_ = nullptr;
      pixel_deserializer_ = nullptr;
      break;
    case Mode::DESERIALIZATION:
      pixel_serializer_ = nullptr;
      pixel_deserializer_ = &NoopDecoder;
      break;
    default:
      NOTREACHED() << "Unknown serialization mode";
  }
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
