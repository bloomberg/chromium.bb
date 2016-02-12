// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PROTO_IMAGE_SERIALIZATION_PROCESSOR_H_
#define CC_PROTO_IMAGE_SERIALIZATION_PROCESSOR_H_

#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelSerializer.h"

class SkPixelSerializer;

namespace cc {

// ImageSerializationProcessor provides functionality to serialize and
// deserialize Skia images.
class ImageSerializationProcessor {
 public:
  // The serializer returned from this function can be used to pass in to
  // SkPicture::serialize(...) for serializing the SkPicture to a stream.
  virtual SkPixelSerializer* GetPixelSerializer() = 0;

  // Returns a function pointer valid to use for deserializing images when using
  // SkPicture::CreateFromStream to create an SkPicture from a stream.
  virtual SkPicture::InstallPixelRefProc GetPixelDeserializer() = 0;
};

}  // namespace cc

#endif  // CC_PROTO_IMAGE_SERIALIZATION_PROCESSOR_H_
