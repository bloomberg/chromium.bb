// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_

#include <stdint.h>

// These data types are defined in va/va.h using typedef, reproduced here.
// TODO(andrescj): revisit this once VaSurfaceFormatToImageFormat() and
// VaSurfaceFormatForJpeg() are moved to the anonymous namespace in the .cc
// file.
using VAImageFormat = struct _VAImageFormat;
using VASurfaceID = unsigned int;

namespace media {

struct JpegFrameHeader;
struct JpegParseResult;
class VaapiWrapper;

// Convert the specified surface format to the associated output image format.
bool VaSurfaceFormatToImageFormat(uint32_t va_rt_format,
                                  VAImageFormat* va_image_format);

unsigned int VaSurfaceFormatForJpeg(const JpegFrameHeader& frame_header);

class VaapiJpegDecoder {
 public:
  VaapiJpegDecoder() = default;
  virtual ~VaapiJpegDecoder() = default;

  // Decodes a JPEG picture. It will fill VA-API parameters and call
  // corresponding VA-API methods according to the JPEG |parse_result|. Decoded
  // data will be outputted to the given |va_surface|. Returns false on failure.
  // |vaapi_wrapper| should be initialized in kDecode mode with
  // VAProfileJPEGBaseline profile. |va_surface| should be created with size at
  // least as large as the picture size.
  static bool DoDecode(VaapiWrapper* vaapi_wrapper,
                       const JpegParseResult& parse_result,
                       VASurfaceID va_surface);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_
