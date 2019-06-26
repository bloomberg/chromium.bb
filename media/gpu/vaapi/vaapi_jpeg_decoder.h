// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "ui/gfx/geometry/size.h"

namespace media {

struct JpegFrameHeader;
class ScopedVAImage;

constexpr unsigned int kInvalidVaRtFormat = 0u;

// Returns the internal format required for a JPEG image given its parsed
// |frame_header|. If the image's subsampling format is not one of 4:2:0, 4:2:2,
// or 4:4:4, returns kInvalidVaRtFormat.
unsigned int VaSurfaceFormatForJpeg(const JpegFrameHeader& frame_header);

// Initializes a VaapiWrapper for the purpose of performing
// hardware-accelerated JPEG decodes.
class VaapiJpegDecoder : public VaapiImageDecoder {
 public:
  VaapiJpegDecoder();
  ~VaapiJpegDecoder() override;

  // VaapiImageDecoder implementation.
  scoped_refptr<VASurface> Decode(base::span<const uint8_t> encoded_image,
                                  VaapiImageDecodeStatus* status) override;
  gpu::ImageDecodeAcceleratorType GetType() const override;

  // Get the decoded data from the last Decode() call as a ScopedVAImage. The
  // VAImage's format will be either |preferred_image_fourcc| if the conversion
  // from the internal format is supported or a fallback FOURCC (see
  // VaapiWrapper::GetJpegDecodeSuitableImageFourCC() for details). Returns
  // nullptr on failure and sets *|status| to the reason for failure.
  std::unique_ptr<ScopedVAImage> GetImage(uint32_t preferred_image_fourcc,
                                          VaapiImageDecodeStatus* status);

 private:
  // The current VA surface for decoding.
  VASurfaceID va_surface_id_;
  // The coded size associated with |va_surface_id_|.
  gfx::Size coded_size_;
  // The VA RT format associated with |va_surface_id_|.
  unsigned int va_rt_format_;

  DISALLOW_COPY_AND_ASSIGN(VaapiJpegDecoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_JPEG_DECODER_H_
