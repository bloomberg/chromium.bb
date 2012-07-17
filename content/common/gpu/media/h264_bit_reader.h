// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_H264_BIT_READER_H_
#define CONTENT_COMMON_GPU_MEDIA_H264_BIT_READER_H_

#include "base/compiler_specific.h"
#include "media/base/bit_reader.h"

namespace content {

// A class to provide bit-granularity reading of H.264 streams.
// This class takes into account H.264 stream-specific constraints, such as
// skipping emulation-prevention bytes and stop bits. See spec for more
// details.
class H264BitReader : public media::BitReader {
 public:
  H264BitReader();
  virtual ~H264BitReader();

 private:
  // This function handles the H.264 escape sequence and stop bit.
  virtual void UpdateCurrByte() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(H264BitReader);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_H264_BIT_READER_H_
