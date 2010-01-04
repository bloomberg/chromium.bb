// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    case kUnknown:
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
    case kUnknown:
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
    case kNumTypes:
    case kMake32Bit:
    default:
      LOG(FATAL) << "Invalid type.";
      return 0;
  }
}

}  // namespace effect_param

}  // namespace gpu
