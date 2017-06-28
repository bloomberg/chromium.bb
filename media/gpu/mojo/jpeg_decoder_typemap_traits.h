// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MOJO_JPEG_DECODER_TYPEMAP_TRAITS_H_
#define MEDIA_GPU_MOJO_JPEG_DECODER_TYPEMAP_TRAITS_H_

#include "media/gpu/mojo/jpeg_decoder.mojom.h"
#include "media/video/jpeg_decode_accelerator.h"

namespace mojo {

template <>
struct EnumTraits<media::mojom::DecodeError,
                  media::JpegDecodeAccelerator::Error> {
  static media::mojom::DecodeError ToMojom(
      media::JpegDecodeAccelerator::Error error);

  static bool FromMojom(media::mojom::DecodeError input,
                        media::JpegDecodeAccelerator::Error* out);
};

}  // namespace mojo

#endif  // MEDIA_GPU_MOJO_JPEG_DECODER_TYPEMAP_TRAITS_H_
