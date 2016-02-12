// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_image_serialization_processor.h"

#include "third_party/skia/include/core/SkPicture.h"

namespace {
bool NoopDecoder(const void* input, size_t input_size, SkBitmap* bitmap) {
  return false;
}
}

class SkPixelSerializer;

namespace cc {

FakeImageSerializationProcessor::FakeImageSerializationProcessor() {}

FakeImageSerializationProcessor::~FakeImageSerializationProcessor() {}

SkPixelSerializer* FakeImageSerializationProcessor::GetPixelSerializer() {
  return nullptr;
}

SkPicture::InstallPixelRefProc
FakeImageSerializationProcessor::GetPixelDeserializer() {
  return &NoopDecoder;
}

}  // namespace cc
