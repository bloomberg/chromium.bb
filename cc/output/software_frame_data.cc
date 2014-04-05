// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/software_frame_data.h"

namespace cc {

SoftwareFrameData::SoftwareFrameData()
    : id(0),
      handle(base::SharedMemory::NULLHandle()) {}

SoftwareFrameData::~SoftwareFrameData() {}

size_t SoftwareFrameData::SizeInBytes() const {
  size_t bytes_per_pixel = 4;
  size_t width = size.width();
  size_t height = size.height();
  return bytes_per_pixel * width * height;
}

base::CheckedNumeric<size_t> SoftwareFrameData::CheckedSizeInBytes() const {
  return base::CheckedNumeric<size_t>(4) *
         base::CheckedNumeric<size_t>(size.width()) *
         base::CheckedNumeric<size_t>(size.height());
}

}  // namespace cc
