// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/feature/compositor/client_image_serialization_processor.h"

#include "blimp/client/feature/compositor/blimp_image_decoder.h"
#include "third_party/skia/include/core/SkPicture.h"

class SkPixelSerializer;

namespace blimp {
namespace client {

ClientImageSerializationProcessor::ClientImageSerializationProcessor() {
  pixel_deserializer_ = &BlimpImageDecoder;
}

ClientImageSerializationProcessor::~ClientImageSerializationProcessor() {}

SkPixelSerializer* ClientImageSerializationProcessor::GetPixelSerializer() {
  return nullptr;
}

SkPicture::InstallPixelRefProc
ClientImageSerializationProcessor::GetPixelDeserializer() {
  return pixel_deserializer_;
}

}  // namespace client
}  // namespace blimp
