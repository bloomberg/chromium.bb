// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TEXTURE_COMPRESSOR_ETC1_H_
#define CC_RESOURCES_TEXTURE_COMPRESSOR_ETC1_H_

#include "cc/resources/texture_compressor.h"

namespace cc {

class CC_EXPORT TextureCompressorETC1 : public TextureCompressor {
 public:
  TextureCompressorETC1() {}

  // Compress a texture using ETC1. Note that the |quality| parameter is
  // ignored. The current implementation does not support different quality
  // settings.
  void Compress(const uint8_t* src,
                uint8_t* dst,
                int width,
                int height,
                Quality quality) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextureCompressorETC1);
};

}  // namespace cc

#endif  // CC_RESOURCES_TEXTURE_COMPRESSOR_ETC1_H_
