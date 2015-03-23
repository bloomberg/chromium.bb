// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/texture_compressor.h"

#include "base/logging.h"
#include "cc/resources/texture_compressor_etc1.h"

namespace cc {

scoped_ptr<TextureCompressor> TextureCompressor::Create(Format format) {
  switch (format) {
    case kFormatETC1:
      return make_scoped_ptr(new TextureCompressorETC1());
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace cc
