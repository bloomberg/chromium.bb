// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_VAAPI_JPEG_DECODER_H_
#define CONTENT_COMMON_GPU_MEDIA_VAAPI_JPEG_DECODER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/gpu/media/vaapi_wrapper.h"

namespace media {
struct JpegParseResult;
}  // namespace media

namespace content {

// A JPEG decoder that utilizes VA-API hardware video decode acceleration on
// Intel systems. Provides functionality to allow plugging VAAPI HW
// acceleration into the JpegDecodeAccelerator framework.
//
// Clients of this class are expected to manage VA surfaces created via
// VaapiWrapper, parse JPEG picture via media::ParseJpegPicture, and then pass
// them to this class.
class CONTENT_EXPORT VaapiJpegDecoder {
 public:
  // Decode a JPEG picture. It will fill VA-API parameters and call
  // corresponding VA-API methods according to parsed JPEG result
  // |parse_result|. Decoded data will be outputted to the given |va_surface|.
  // Return false on failure.
  // |vaapi_wrapper| should be initialized in kDecode mode with
  // VAProfileJPEGBaseline profile.
  // |va_surface| should be created with size at least as large as the picture
  // size.
  static bool Decode(VaapiWrapper* vaapi_wrapper,
                     const media::JpegParseResult& parse_result,
                     VASurfaceID va_surface);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(VaapiJpegDecoder);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_VAAPI_JPEG_DECODER_H_
