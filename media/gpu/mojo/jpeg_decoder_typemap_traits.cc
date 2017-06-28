// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/mojo/jpeg_decoder_typemap_traits.h"

#include "base/logging.h"

namespace mojo {

// static
media::mojom::DecodeError
EnumTraits<media::mojom::DecodeError, media::JpegDecodeAccelerator::Error>::
    ToMojom(media::JpegDecodeAccelerator::Error error) {
  switch (error) {
    case media::JpegDecodeAccelerator::NO_ERRORS:
      return media::mojom::DecodeError::NO_ERRORS;
    case media::JpegDecodeAccelerator::INVALID_ARGUMENT:
      return media::mojom::DecodeError::INVALID_ARGUMENT;
    case media::JpegDecodeAccelerator::UNREADABLE_INPUT:
      return media::mojom::DecodeError::UNREADABLE_INPUT;
    case media::JpegDecodeAccelerator::PARSE_JPEG_FAILED:
      return media::mojom::DecodeError::PARSE_JPEG_FAILED;
    case media::JpegDecodeAccelerator::UNSUPPORTED_JPEG:
      return media::mojom::DecodeError::UNSUPPORTED_JPEG;
    case media::JpegDecodeAccelerator::PLATFORM_FAILURE:
      return media::mojom::DecodeError::PLATFORM_FAILURE;
  }
  NOTREACHED();
  return media::mojom::DecodeError::NO_ERRORS;
}

// static
bool EnumTraits<media::mojom::DecodeError,
                media::JpegDecodeAccelerator::Error>::
    FromMojom(media::mojom::DecodeError error,
              media::JpegDecodeAccelerator::Error* out) {
  switch (error) {
    case media::mojom::DecodeError::NO_ERRORS:
      *out = media::JpegDecodeAccelerator::Error::NO_ERRORS;
      return true;
    case media::mojom::DecodeError::INVALID_ARGUMENT:
      *out = media::JpegDecodeAccelerator::Error::INVALID_ARGUMENT;
      return true;
    case media::mojom::DecodeError::UNREADABLE_INPUT:
      *out = media::JpegDecodeAccelerator::Error::UNREADABLE_INPUT;
      return true;
    case media::mojom::DecodeError::PARSE_JPEG_FAILED:
      *out = media::JpegDecodeAccelerator::Error::PARSE_JPEG_FAILED;
      return true;
    case media::mojom::DecodeError::UNSUPPORTED_JPEG:
      *out = media::JpegDecodeAccelerator::Error::UNSUPPORTED_JPEG;
      return true;
    case media::mojom::DecodeError::PLATFORM_FAILURE:
      *out = media::JpegDecodeAccelerator::Error::PLATFORM_FAILURE;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
