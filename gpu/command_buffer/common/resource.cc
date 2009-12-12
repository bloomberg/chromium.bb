/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the implementation of the helper functions for resources.

#include "gpu/command_buffer/common/resource.h"

namespace gpu {

namespace texture {

// Gets the number of bytes per block for a given format.
unsigned int GetBytesPerBlock(Format format) {
  switch (format) {
    case kXRGB8:
    case kARGB8:
    case kR32F:
      return 4;
    case kABGR16F:
      return 8;
    case kABGR32F:
      return 16;
    case kDXT1:
      return 8;
    default:
      // TODO(petersont): Add DXT3/5 support.
      LOG(FATAL) << "Invalid format";
      return 1;
  }
}

// Gets the width of a block for a given format.
unsigned int GetBlockSizeX(Format format) {
  switch (format) {
    case kXRGB8:
    case kARGB8:
    case kABGR16F:
    case kR32F:
    case kABGR32F:
      return 1;
    case kDXT1:
      return 4;
    default:
      // TODO(petersont): Add DXT3/5 support.
      LOG(FATAL) << "Invalid format";
      return 1;
  }
}

// Gets the height of a block for a given format.
unsigned int GetBlockSizeY(Format format) {
  // NOTE: currently only supported formats use square blocks.
  return GetBlockSizeX(format);
}

}  // namespace texture

namespace effect_param {

// Gets the size of the data of a given parameter type.
unsigned int GetDataSize(DataType type) {
  switch (type) {
    case kUnknown:
      return 0;
    case kFloat1:
      return sizeof(float);  // NOLINT
    case kFloat2:
      return sizeof(float) * 2;  // NOLINT
    case kFloat3:
      return sizeof(float) * 3;  // NOLINT
    case kFloat4:
      return sizeof(float) * 4;  // NOLINT
    case kMatrix4:
      return sizeof(float) * 16;  // NOLINT
    case kInt:
      return sizeof(int);  // NOLINT
    case kBool:
      return sizeof(bool);  // NOLINT
    case kSampler:
      return sizeof(ResourceId);  // NOLINT
    case kTexture:
      return sizeof(ResourceId);  // NOLINT
    default:
      LOG(FATAL) << "Invalid type.";
      return 0;
  }
}

}  // namespace effect_param

}  // namespace gpu
